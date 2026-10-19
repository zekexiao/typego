package release

import (
	"errors"
	"net/url"
	"strings"
	"time"
)

type Platform string

const (
	Windows Platform = "windows"
	MacOS   Platform = "macos"
	Linux   Platform = "linux"
)

// Release 描述一个平台版本及其发布策略。
type Release struct {
	ID             string    `json:"id"`
	Platform       Platform  `json:"platform"`
	Arch           string    `json:"arch"`
	Version        string    `json:"version"`
	DownloadURL    string    `json:"download_url"`
	Changelog      string    `json:"changelog"`
	Mandatory      bool      `json:"mandatory"`
	Published      bool      `json:"published"`
	RolloutPercent int       `json:"rollout_percent"`
	PublishedAt    time.Time `json:"published_at"`
	CreatedAt      time.Time `json:"created_at"`
	UpdatedAt      time.Time `json:"updated_at"`
}

func (r *Release) Normalize(now time.Time) {
	r.Platform = Platform(strings.ToLower(strings.TrimSpace(string(r.Platform))))
	r.Arch = strings.ToLower(strings.TrimSpace(r.Arch))
	r.Version = strings.TrimSpace(r.Version)
	r.DownloadURL = strings.TrimSpace(r.DownloadURL)
	r.Changelog = strings.TrimSpace(r.Changelog)
	if r.RolloutPercent < 0 {
		r.RolloutPercent = 0
	}
	if r.RolloutPercent > 100 {
		r.RolloutPercent = 100
	}
	if r.CreatedAt.IsZero() {
		r.CreatedAt = now.UTC()
	}
	r.UpdatedAt = now.UTC()
}

func (r Release) Validate() error {
	if r.Platform != Windows && r.Platform != MacOS && r.Platform != Linux {
		return errors.New("platform must be windows, macos, or linux")
	}
	if r.Arch == "" || r.Version == "" || r.DownloadURL == "" {
		return errors.New("arch, version, and download_url are required")
	}
	downloadURL, err := url.Parse(r.DownloadURL)
	if err != nil || (downloadURL.Scheme != "https" && downloadURL.Scheme != "http") || downloadURL.Host == "" {
		return errors.New("download_url must be an absolute HTTP or HTTPS URL")
	}
	if r.RolloutPercent < 0 || r.RolloutPercent > 100 {
		return errors.New("rollout_percent must be between 0 and 100")
	}
	return nil
}
