package httpapi

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"path"
	"strconv"
	"strings"
	"time"
)

const (
	uploadAPIEndpoint    = "https://cn-nb1.rains3.com"
	uploadPublicEndpoint = "https://typego.cn-nb1.rains3.com"
	uploadRegion         = "cn-nb1"
	maxUploadBytes       = 2 << 30
)

type presignRequest struct {
	AccessKey string `json:"access_key"`
	SecretKey string `json:"secret_key"`
	Bucket    string `json:"bucket"`
	ObjectKey string `json:"object_key"`
}

func (a *api) presignUpload(w http.ResponseWriter, r *http.Request) {
	var input presignRequest
	if err := decodeJSON(w, r, &input); err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}
	input.AccessKey = strings.TrimSpace(input.AccessKey)
	input.SecretKey = strings.TrimSpace(input.SecretKey)
	input.Bucket = strings.TrimSpace(input.Bucket)
	input.ObjectKey = cleanObjectKey(input.ObjectKey)
	if input.AccessKey == "" || input.SecretKey == "" || input.Bucket == "" || input.ObjectKey == "" {
		writeError(w, http.StatusBadRequest, "access_key, secret_key, bucket, and object_key are required")
		return
	}
	uploadURL, publicURL := presignedPutURL(input.AccessKey, input.SecretKey, input.Bucket, input.ObjectKey, 15*time.Minute)
	writeJSON(w, http.StatusOK, map[string]string{
		"upload_url":   uploadURL,
		"download_url": publicURL,
		"object_key":   input.ObjectKey,
	})
}

func (a *api) upload(w http.ResponseWriter, r *http.Request) {
	r.Body = http.MaxBytesReader(w, r.Body, maxUploadBytes)
	if err := r.ParseMultipartForm(32 << 20); err != nil {
		writeError(w, http.StatusBadRequest, "invalid multipart form: "+err.Error())
		return
	}
	defer r.MultipartForm.RemoveAll()

	accessKey := strings.TrimSpace(r.FormValue("access_key"))
	secretKey := strings.TrimSpace(r.FormValue("secret_key"))
	bucket := strings.TrimSpace(r.FormValue("bucket"))
	objectKey := cleanObjectKey(r.FormValue("object_key"))
	if accessKey == "" || secretKey == "" || bucket == "" {
		writeError(w, http.StatusBadRequest, "access_key, secret_key, and bucket are required")
		return
	}

	file, header, err := r.FormFile("file")
	if err != nil {
		writeError(w, http.StatusBadRequest, "file is required")
		return
	}
	defer file.Close()
	if objectKey == "" {
		objectKey = cleanObjectKey(header.Filename)
	}
	if objectKey == "" {
		writeError(w, http.StatusBadRequest, "invalid object_key")
		return
	}

	contentType := header.Header.Get("Content-Type")
	if contentType == "" {
		contentType = "application/octet-stream"
	}
	publicURL, err := uploadS3(r, accessKey, secretKey, bucket, objectKey, contentType, file, header.Size)
	if err != nil {
		writeError(w, http.StatusBadGateway, err.Error())
		return
	}
	accessible := publicAccessible(r, publicURL)
	response := map[string]any{
		"download_url":      publicURL,
		"object_key":        objectKey,
		"public_accessible": accessible,
	}
	if !accessible {
		response["warning"] = "upload succeeded, but the public URL is not accessible; configure the bucket for public read access"
	}
	writeJSON(w, http.StatusOK, response)
}

func presignedPutURL(accessKey, secretKey, bucket, objectKey string, validFor time.Duration) (string, string) {
	endpoint, _ := url.Parse(uploadAPIEndpoint)
	objectPath := "/" + bucket + "/" + escapePath(objectKey)
	now := time.Now().UTC()
	date := now.Format("20060102")
	dateTime := now.Format("20060102T150405Z")
	scope := date + "/" + uploadRegion + "/s3/aws4_request"

	query := url.Values{}
	query.Set("X-Amz-Algorithm", "AWS4-HMAC-SHA256")
	query.Set("X-Amz-Credential", accessKey+"/"+scope)
	query.Set("X-Amz-Date", dateTime)
	query.Set("X-Amz-Expires", strconv.FormatInt(int64(validFor/time.Second), 10))
	query.Set("X-Amz-SignedHeaders", "host")
	canonicalQuery := query.Encode()
	canonicalRequest := strings.Join([]string{
		http.MethodPut,
		objectPath,
		canonicalQuery,
		"host:" + endpoint.Host + "\n",
		"host",
		"UNSIGNED-PAYLOAD",
	}, "\n")
	stringToSign := "AWS4-HMAC-SHA256\n" + dateTime + "\n" + scope + "\n" + hashHex([]byte(canonicalRequest))
	query.Set("X-Amz-Signature", hex.EncodeToString(hmacSHA256(signingKey(secretKey, date), stringToSign)))

	target := *endpoint
	target.Path = "/" + bucket + "/" + objectKey
	target.RawPath = objectPath
	target.RawQuery = query.Encode()
	publicURL := strings.TrimRight(uploadPublicEndpoint, "/") + "/" + escapePath(objectKey)
	return target.String(), publicURL
}

func uploadS3(source *http.Request, accessKey, secretKey, bucket, objectKey, contentType string, body io.ReadSeeker, size int64) (string, error) {
	endpoint, _ := url.Parse(uploadAPIEndpoint)
	objectPath := "/" + bucket + "/" + escapePath(objectKey)
	target := *endpoint
	target.Path = "/" + bucket + "/" + objectKey
	target.RawPath = objectPath
	now := time.Now().UTC()
	date := now.Format("20060102")
	dateTime := now.Format("20060102T150405Z")
	hasher := sha256.New()
	if _, err := io.Copy(hasher, body); err != nil {
		return "", fmt.Errorf("hash upload: %w", err)
	}
	payloadHash := hex.EncodeToString(hasher.Sum(nil))
	if _, err := body.Seek(0, io.SeekStart); err != nil {
		return "", fmt.Errorf("rewind upload: %w", err)
	}

	canonicalHeaders := "content-type:" + contentType + "\n" +
		"host:" + target.Host + "\n" +
		"x-amz-acl:public-read\n" +
		"x-amz-content-sha256:" + payloadHash + "\n" +
		"x-amz-date:" + dateTime + "\n"
	signedHeaders := "content-type;host;x-amz-acl;x-amz-content-sha256;x-amz-date"
	canonicalRequest := strings.Join([]string{
		http.MethodPut, objectPath, "", canonicalHeaders, signedHeaders, payloadHash,
	}, "\n")
	scope := date + "/" + uploadRegion + "/s3/aws4_request"
	stringToSign := "AWS4-HMAC-SHA256\n" + dateTime + "\n" + scope + "\n" + hashHex([]byte(canonicalRequest))
	signature := hex.EncodeToString(hmacSHA256(signingKey(secretKey, date), stringToSign))

	request, err := http.NewRequestWithContext(source.Context(), http.MethodPut, target.String(), body)
	if err != nil {
		return "", err
	}
	request.Header.Set("Content-Type", contentType)
	request.Header.Set("X-Amz-Acl", "public-read")
	request.Header.Set("X-Amz-Content-Sha256", payloadHash)
	request.Header.Set("X-Amz-Date", dateTime)
	request.Header.Set("Content-Length", strconv.FormatInt(size, 10))
	request.ContentLength = size
	request.Header.Set("Authorization", "AWS4-HMAC-SHA256 Credential="+accessKey+"/"+scope+
		", SignedHeaders="+signedHeaders+", Signature="+signature)

	response, err := http.DefaultClient.Do(request)
	if err != nil {
		return "", fmt.Errorf("upload request failed: %w", err)
	}
	defer response.Body.Close()
	if response.StatusCode < 200 || response.StatusCode >= 300 {
		message, _ := io.ReadAll(io.LimitReader(response.Body, 4096))
		return "", errors.New("S3 upload failed: " + response.Status + " " + strings.TrimSpace(string(message)))
	}
	return strings.TrimRight(uploadPublicEndpoint, "/") + "/" + escapePath(objectKey), nil
}

func signingKey(secretKey, date string) []byte {
	dateKey := hmacSHA256([]byte("AWS4"+secretKey), date)
	regionKey := hmacSHA256(dateKey, uploadRegion)
	serviceKey := hmacSHA256(regionKey, "s3")
	return hmacSHA256(serviceKey, "aws4_request")
}

func hmacSHA256(key []byte, value string) []byte {
	mac := hmac.New(sha256.New, key)
	_, _ = mac.Write([]byte(value))
	return mac.Sum(nil)
}

func hashHex(value []byte) string {
	sum := sha256.Sum256(value)
	return hex.EncodeToString(sum[:])
}

func cleanObjectKey(value string) string {
	value = strings.TrimSpace(strings.ReplaceAll(value, "\\", "/"))
	value = strings.TrimPrefix(path.Clean("/"+value), "/")
	if value == "." || strings.HasPrefix(value, "../") {
		return ""
	}
	return value
}

func escapePath(value string) string {
	parts := strings.Split(value, "/")
	for i := range parts {
		parts[i] = url.PathEscape(parts[i])
	}
	return strings.Join(parts, "/")
}

func publicAccessible(source *http.Request, publicURL string) bool {
	request, err := http.NewRequestWithContext(source.Context(), http.MethodHead, publicURL, nil)
	if err != nil {
		return false
	}
	response, err := http.DefaultClient.Do(request)
	if err != nil {
		return false
	}
	defer response.Body.Close()
	return response.StatusCode >= 200 && response.StatusCode < 400
}
