package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"posts-service/internal/graph"
	"posts-service/internal/repository"

	"github.com/99designs/gqlgen/graphql"
	"github.com/99designs/gqlgen/graphql/handler"
	"github.com/99designs/gqlgen/graphql/playground"
)

func run() error {
	postRepo, commentRepo, closeFn, err := initStorage()
	if err != nil {
		return err
	}
	defer closeFn()

	srv := newHTTPServer(postRepo, commentRepo)

	go listenForShutdown(srv)

	port := envOr("PORT", "8080")
	log.Printf("Server on http://localhost:%s/ (storage: %s)", port, envOr("STORAGE", "memory"))

	if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		return err
	}

	log.Println("Server stopped")
	return nil
}

func initStorage() (repository.PostRepository, repository.CommentRepository, func(), error) {
	storage := envOr("STORAGE", "memory")
	log.Printf("Starting with storage: %s", storage)

	if storage == "postgres" {
		return initPostgres()
	}
	return initMemory()
}

func initPostgres() (repository.PostRepository, repository.CommentRepository, func(), error) {
	connString := envOr("DATABASE_URL", "postgresql://postgres@localhost:5432/postsdb?sslmode=disable")
	pgStore, err := repository.NewPostgresStore(connString)
	if err != nil {
		return nil, nil, nil, err
	}
	log.Println("PostgreSQL connected")
	return pgStore, pgStore, pgStore.Close, nil
}

func initMemory() (repository.PostRepository, repository.CommentRepository, func(), error) {
	mem := repository.NewMemoryStore()
	log.Println("Memory store ready")
	return mem, mem, func() {}, nil
}

func newHTTPServer(postRepo repository.PostRepository, commentRepo repository.CommentRepository) *http.Server {
	resolver := graph.NewResolver(postRepo, commentRepo)

	gqlSrv := handler.NewDefaultServer(
		graph.NewExecutableSchema(graph.Config{Resolvers: resolver}),
	)
	gqlSrv.AroundOperations(func(ctx context.Context, next graphql.OperationHandler) graphql.ResponseHandler {
		ctx = resolver.DataLoaderMiddleware(ctx)
		return next(ctx)
	})

	mux := http.NewServeMux()
	mux.Handle("/", playground.Handler("GraphQL playground", "/query"))
	mux.Handle("/query", gqlSrv)

	return &http.Server{
		Addr:         ":" + envOr("PORT", "8080"),
		Handler:      mux,
		ReadTimeout:  15 * time.Second,
		WriteTimeout: 15 * time.Second,
		IdleTimeout:  60 * time.Second,
	}
}

func listenForShutdown(srv *http.Server) {
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Println("Shutting down...")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	if err := srv.Shutdown(ctx); err != nil {
		log.Printf("Shutdown error: %v", err)
	}
}

func envOr(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	log.Printf("%s not set, defaulting to %q", key, fallback)
	return fallback
}