# Org API

REST API для управления организационной структурой: дерево подразделений и сотрудники. Поддерживает иерархию с защитой от циклов, каскадное удаление и перенос сотрудников.

## Стек

Go 1.23 (`net/http`), GORM, PostgreSQL 16, goose (миграции), Docker, testify.

## Запуск

```bash
docker compose up --build
```

API на `http://localhost:8080`. Миграции применяются автоматически при старте.

## Переменные окружения

| Переменная | По умолчанию | Описание |
|---|---|---|
| `DB_HOST` | localhost | Хост PostgreSQL |
| `DB_PORT` | 5432 | Порт PostgreSQL |
| `DB_USER` | postgres | Пользователь БД |
| `DB_PASSWORD` | postgres | Пароль БД |
| `DB_NAME` | orgapi | Имя базы данных |
| `SERVER_PORT` | 8080 | Порт HTTP-сервера |
| `DB_MAX_OPEN_CONNS` | 25 | Макс. открытых соединений с БД |
| `DB_MAX_IDLE_CONNS` | 5 | Макс. простаивающих соединений |

## Модель данных

- **departments** — `id`, `name` (≤200), `parent_id` (`NULL` для корня), `created_at`. Имя уникально в рамках родителя; удаление каскадно удаляет потомков.
- **employees** — `id`, `department_id`, `full_name` (≤200), `position` (≤200), `hired_at` (опц.), `created_at`. Удаляются вместе с подразделением.

## Эндпоинты

### `POST /departments/` — создать подразделение

```json
{ "name": "Engineering", "parent_id": null }
```

`parent_id` опционален (`null`/отсутствует — корень). → `201`

### `GET /departments/{id}` — получить подразделение с поддеревом

Query: `depth` (1–5, по умолч. 1), `include_employees` (по умолч. true), `sort_by` (`created_at` | `full_name`).
Пример: `GET /departments/1?depth=3&sort_by=full_name`. Возвращает дерево с `children` и `employees`.

### `PATCH /departments/{id}` — обновить

Передаются только изменяемые поля. `"parent_id": null` делает подразделение корневым. Перемещение, создающее цикл, отклоняется (`409`). Выполняется в SERIALIZABLE-транзакции.

```json
{ "name": "Platform", "parent_id": 5 }
```

### `DELETE /departments/{id}` — удалить

Query: `mode=cascade` (с потомками и сотрудниками) или `mode=reassign` (перенести сотрудников, тогда обязателен `reassign_to_department_id`).
Примеры: `?mode=cascade`, `?mode=reassign&reassign_to_department_id=2`. → `204`

### `POST /departments/{id}/employees/` — добавить сотрудника

```json
{ "full_name": "Иван Иванов", "position": "Engineer", "hired_at": "2024-01-15T00:00:00Z" }
```

`hired_at` опционален. → `201` (или `404`, если подразделения нет).

## Ошибки

Формат `{"error": "..."}`. Статусы: `400` (валидация/параметры), `404` (не найдено), `409` (дубль имени, цикл), `500` (внутренняя/паника).

## Middleware

Каждый запрос: recovery от паник (→ 500), `X-Request-ID` в заголовке ответа, JSON-логирование (метод, путь, статус, длительность, request_id).

## Тесты

```bash
go test ./...
```

Покрыты хендлеры (включая конкурентные сценарии) и бизнес-логика сервисов: валидация, дубли, циклы, ограничение глубины, режимы удаления.
