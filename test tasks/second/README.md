# Org API

REST API для управления организационной структурой: подразделения и сотрудники.

## Стек

- Go 1.23, net/http, GORM, goose, Docker, PostgreSQL 16

## Запуск

```bash
docker compose up --build
```

API доступен на `http://localhost:8080`. Миграции применяются автоматически.

## Тесты

```bash
go test ./...
```