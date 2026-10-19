package httpapi

import (
	"crypto/subtle"
	"embed"
	"encoding/json"
	"errors"
	"log"
	"net/http"
	"strings"
	"time"

	"typego/server/internal/release"
)

const maxBodyBytes = 1 << 20

//go:embed admin/*
var adminFiles embed.FS

type api struct {
	store      release.Store
	adminToken string
}

func New(store release.Store, adminToken string) http.Handler {
	a := &api{store: store, adminToken: adminToken}
	mux := http.NewServeMux()
	mux.HandleFunc("GET /healthz", a.health)
	mux.HandleFunc("GET /api/v1/releases/latest", a.latest)
	mux.HandleFunc("GET /api/v1/update/latest", a.updateLatest)
	mux.HandleFunc("GET /download/{platform}", a.download)
	mux.HandleFunc("GET /admin", func(w http.ResponseWriter, r *http.Request) {
		http.Redirect(w, r, "/admin/", http.StatusTemporaryRedirect)
	})
	mux.Handle("GET /admin/", noCache(http.FileServerFS(adminFiles)))
	mux.Handle("GET /admin/api/v1/releases", a.admin(a.list))
	mux.Handle("PUT /admin/api/v1/releases", a.admin(a.upsert))
	mux.Handle("DELETE /admin/api/v1/releases/{id}", a.admin(a.delete))
	mux.Handle("POST /admin/api/v1/uploads", a.admin(a.upload))
	mux.Handle("POST /admin/api/v1/uploads/presign", a.admin(a.presignUpload))
	mux.HandleFunc("/api/v1/users/", notImplemented)
	mux.HandleFunc("/api/v1/subscriptions/", notImplemented)
	return logging(mux)
}

func (a *api) health(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, map[string]string{"status": "ok"})
}

func (a *api) latest(w http.ResponseWriter, r *http.Request) {
	item, ok := a.findLatest(r)
	if !ok {
		writeError(w, http.StatusNotFound, "no release available")
		return
	}
	writeJSON(w, http.StatusOK, item)
}

// updateLatest 保持与桌面端更新器当前字段兼容。
func (a *api) updateLatest(w http.ResponseWriter, r *http.Request) {
	item, ok := a.findLatest(r)
	if !ok {
		writeJSON(w, http.StatusOK, map[string]any{"has_update": false})
		return
	}
	current := r.URL.Query().Get("current_version")
	writeJSON(w, http.StatusOK, map[string]any{
		"has_update":   current == "" || current != item.Version,
		"version":      item.Version,
		"download_url": item.DownloadURL,
		"changelog":    item.Changelog,
		"mandatory":    item.Mandatory,
		"platform":     item.Platform,
		"arch":         item.Arch,
		"published_at": item.PublishedAt,
	})
}

func (a *api) download(w http.ResponseWriter, r *http.Request) {
	item, ok := a.findLatest(r)
	if !ok {
		writeError(w, http.StatusNotFound, "no release available")
		return
	}
	http.Redirect(w, r, item.DownloadURL, http.StatusTemporaryRedirect)
}

func (a *api) findLatest(r *http.Request) (release.Release, bool) {
	platform := release.Platform(strings.ToLower(r.PathValue("platform")))
	if platform == "" {
		platform = release.Platform(strings.ToLower(r.URL.Query().Get("platform")))
	}
	return a.store.Latest(platform, strings.ToLower(r.URL.Query().Get("arch")), clientID(r))
}

func (a *api) list(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, map[string]any{"releases": a.store.List()})
}

func (a *api) upsert(w http.ResponseWriter, r *http.Request) {
	var value release.Release
	if err := decodeJSON(w, r, &value); err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}
	saved, err := a.store.Upsert(value)
	if err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}
	writeJSON(w, http.StatusOK, saved)
}

func (a *api) delete(w http.ResponseWriter, r *http.Request) {
	if err := a.store.Delete(r.PathValue("id")); err != nil {
		if errors.Is(err, release.ErrNotFound) {
			writeError(w, http.StatusNotFound, err.Error())
			return
		}
		writeError(w, http.StatusInternalServerError, err.Error())
		return
	}
	w.WriteHeader(http.StatusNoContent)
}

func (a *api) admin(next http.HandlerFunc) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		token := strings.TrimPrefix(r.Header.Get("Authorization"), "Bearer ")
		if len(token) != len(a.adminToken) ||
			subtle.ConstantTimeCompare([]byte(token), []byte(a.adminToken)) != 1 {
			writeError(w, http.StatusUnauthorized, "unauthorized")
			return
		}
		next(w, r)
	})
}

func clientID(r *http.Request) string {
	if id := r.URL.Query().Get("client_id"); id != "" {
		return id
	}
	return r.Header.Get("X-TypeGo-Client-ID")
}

func decodeJSON(w http.ResponseWriter, r *http.Request, dst any) error {
	r.Body = http.MaxBytesReader(w, r.Body, maxBodyBytes)
	decoder := json.NewDecoder(r.Body)
	decoder.DisallowUnknownFields()
	return decoder.Decode(dst)
}

func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.Header().Set("Cache-Control", "no-store")
	w.WriteHeader(status)
	if err := json.NewEncoder(w).Encode(value); err != nil {
		log.Printf("write response: %v", err)
	}
}

func writeError(w http.ResponseWriter, status int, message string) {
	writeJSON(w, status, map[string]string{"error": message})
}

func notImplemented(w http.ResponseWriter, _ *http.Request) {
	writeError(w, http.StatusNotImplemented, "reserved for future implementation")
}

func logging(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		started := time.Now()
		next.ServeHTTP(w, r)
		log.Printf("%s %s %s", r.Method, r.URL.Path, time.Since(started))
	})
}

func noCache(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-store")
		next.ServeHTTP(w, r)
	})
}
