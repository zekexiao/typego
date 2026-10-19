package main

import (
	"log"
	"net/http"
	"os"
	"time"

	"typego/server/internal/httpapi"
	"typego/server/internal/release"
)

func main() {
	addr := env("TYPEGO_ADDR", ":8080")
	dataFile := env("TYPEGO_DATA_FILE", "data/releases.json")
	adminToken := os.Getenv("TYPEGO_ADMIN_TOKEN")
	if adminToken == "" {
		log.Fatal("TYPEGO_ADMIN_TOKEN must be set")
	}

	store, err := release.NewFileStore(dataFile)
	if err != nil {
		log.Fatalf("initialize release store: %v", err)
	}

	server := &http.Server{
		Addr:              addr,
		Handler:           httpapi.New(store, adminToken),
		ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout:       30 * time.Minute,
		WriteTimeout:      30 * time.Minute,
		IdleTimeout:       60 * time.Second,
	}

	log.Printf("TypeGo server listening on %s", addr)
	if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		log.Fatal(err)
	}
}

func env(name, fallback string) string {
	if value := os.Getenv(name); value != "" {
		return value
	}
	return fallback
}
