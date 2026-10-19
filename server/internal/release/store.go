package release

import (
	"crypto/sha256"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"sync"
	"time"
)

var ErrNotFound = errors.New("release not found")

// Store 定义版本存储边界，后续可替换为数据库实现。
type Store interface {
	List() []Release
	Upsert(Release) (Release, error)
	Delete(string) error
	Latest(Platform, string, string) (Release, bool)
}

// FileStore 使用内存读取和原子 JSON 写入，适合单实例部署。
type FileStore struct {
	mu       sync.RWMutex
	path     string
	releases []Release
}

func NewFileStore(path string) (*FileStore, error) {
	store := &FileStore{path: path}
	data, err := os.ReadFile(path)
	if errors.Is(err, os.ErrNotExist) {
		return store, nil
	}
	if err != nil {
		return nil, err
	}
	if len(data) > 0 {
		if err := json.Unmarshal(data, &store.releases); err != nil {
			return nil, fmt.Errorf("decode %s: %w", path, err)
		}
	}
	return store, nil
}

func (s *FileStore) List() []Release {
	s.mu.RLock()
	defer s.mu.RUnlock()
	result := append([]Release(nil), s.releases...)
	sort.Slice(result, func(i, j int) bool {
		return result[i].PublishedAt.After(result[j].PublishedAt)
	})
	return result
}

func (s *FileStore) Upsert(value Release) (Release, error) {
	now := time.Now()
	value.Normalize(now)
	if value.ID == "" {
		value.ID = fmt.Sprintf("%s-%s-%s", value.Platform, value.Arch, value.Version)
	}
	if err := value.Validate(); err != nil {
		return Release{}, err
	}

	s.mu.Lock()
	defer s.mu.Unlock()
	for i := range s.releases {
		if s.releases[i].ID != value.ID {
			continue
		}
		value.CreatedAt = s.releases[i].CreatedAt
		if value.Published && value.PublishedAt.IsZero() {
			value.PublishedAt = s.releases[i].PublishedAt
		}
		if !value.Published {
			value.PublishedAt = time.Time{}
		}
		if value.PublishedAt.IsZero() && value.Published {
			value.PublishedAt = now.UTC()
		}
		s.releases[i] = value
		return value, s.saveLocked()
	}
	if value.Published && value.PublishedAt.IsZero() {
		value.PublishedAt = now.UTC()
	}
	s.releases = append(s.releases, value)
	return value, s.saveLocked()
}

func (s *FileStore) Delete(id string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	for i := range s.releases {
		if s.releases[i].ID != id {
			continue
		}
		s.releases = append(s.releases[:i], s.releases[i+1:]...)
		return s.saveLocked()
	}
	return ErrNotFound
}

func (s *FileStore) Latest(platform Platform, arch, clientID string) (Release, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	var latest Release
	found := false
	for _, item := range s.releases {
		if !item.Published || item.Platform != platform || (arch != "" && item.Arch != arch) {
			continue
		}
		if item.RolloutPercent < 100 && !rolloutHit(item.ID, clientID, item.RolloutPercent) {
			continue
		}
		if !found || item.PublishedAt.After(latest.PublishedAt) {
			latest, found = item, true
		}
	}
	return latest, found
}

func (s *FileStore) saveLocked() error {
	if err := os.MkdirAll(filepath.Dir(s.path), 0o755); err != nil {
		return err
	}
	data, err := json.MarshalIndent(s.releases, "", "  ")
	if err != nil {
		return err
	}
	tmp := s.path + ".tmp"
	if err := os.WriteFile(tmp, data, 0o600); err != nil {
		return err
	}
	return os.Rename(tmp, s.path)
}

func rolloutHit(releaseID, clientID string, percent int) bool {
	if percent >= 100 {
		return true
	}
	if percent <= 0 || clientID == "" {
		return false
	}
	sum := sha256.Sum256([]byte(releaseID + "\x00" + clientID))
	return binary.BigEndian.Uint32(sum[:4])%100 < uint32(percent)
}
