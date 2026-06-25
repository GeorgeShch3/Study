package main

import (
	"context"
	"fmt"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/pressly/goose/v3"
	"github.com/yourname/org-api/config"
	"github.com/yourname/org-api/internal/handler"
	"github.com/yourname/org-api/internal/repository"
	"github.com/yourname/org-api/internal/service"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
)

func main() {
	logger := slog.New(slog.NewJSONHandler(os.Stdout, nil))

	cfg := config.Load()

	db, err := gorm.Open(postgres.Open(cfg.DSN()), &gorm.Config{})
	if err != nil {
		logger.Error("failed to connect to database", "error", err)
		os.Exit(1)
	}

	sqlDB, err := db.DB()
	if err != nil {
		logger.Error("failed to get sql.DB", "error", err)
		os.Exit(1)
	}

	sqlDB.SetMaxOpenConns(cfg.MaxOpenConns)
	sqlDB.SetMaxIdleConns(cfg.MaxIdleConns)
	sqlDB.SetConnMaxLifetime(5 * time.Minute)

	if err := goose.SetDialect("postgres"); err != nil {
		logger.Error("failed to set goose dialect", "error", err)
		os.Exit(1)
	}
	if err := goose.Up(sqlDB, "migrations"); err != nil {
		logger.Error("failed to run migrations", "error", err)
		os.Exit(1)
	}
	logger.Info("migrations applied successfully")

	deptRepo := repository.NewDepartmentRepository(db)
	empRepo := repository.NewEmployeeRepository(db)

	deptSvc := service.NewDepartmentService(deptRepo, empRepo)
	empSvc := service.NewEmployeeService(deptRepo, empRepo)

	deptHandler := handler.NewDepartmentHandler(deptSvc)
	empHandler := handler.NewEmployeeHandler(empSvc)

	router := handler.NewRouter(logger, deptHandler, empHandler)

	srv := &http.Server{
		Addr:         fmt.Sprintf(":%s", cfg.ServerPort),
		Handler:      router,
		ReadTimeout:  5 * time.Second,
		WriteTimeout: 10 * time.Second,
		IdleTimeout:  60 * time.Second,
	}

	go func() {
		logger.Info("server starting", "port", cfg.ServerPort)
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			logger.Error("server error", "error", err)
			os.Exit(1)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	logger.Info("shutting down server...")

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	if err := srv.Shutdown(ctx); err != nil {
		logger.Error("server forced to shutdown", "error", err)
	}

	logger.Info("server stopped")
}
