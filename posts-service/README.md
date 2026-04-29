
# Posts Service

GraphQL-сервис для постов и комментариев. Поддерживает два режима хранения данных (in-memory и PostgreSQL), real-time подписки и оптимизацию запросов через DataLoader.

## Требования

- Go 1.21+
- Docker 
-  PostgreSQL
## Переменные окружения

| Переменная | По умолчанию | Описание |
|---|---|---|
| STORAGE | memory | Хранилище: memory или postgres |
| PORT | 8080 | Порт HTTP-сервера |
| DATABASE_URL | postgresql://postgres@localhost:5432/postsdb?sslmode=disable | Строка подключения к PostgreSQL |
| TEST_DB_URL | — | Строка подключения для интеграционных тестов |

## Запуск

In-memory:
`go run server/*.go`

PostgreSQL:
`STORAGE=postgres DATABASE_URL="postgresql://postgres:password@localhost:5432/postsdb?sslmode=disable" go run server/*.go`

Docker in-memory:
`docker compose --profile memory up --build`

Docker PostgreSQL:
`docker compose --profile postgres up --build`

Остановка Docker:
`docker compose down`

После запуска GraphQL playground доступен на http://localhost:8080/.

## Примеры запросов

Создать пост:
```
mutation {
  createPost(title: "Привет", content: "Первый пост", author: "Ivan") {
    id
    title
    createdAt
  }
}
```

Получить список постов с комментариями:
```
query {
  posts(first: 10) {
    edges {
      node {
        id
        title
        author
        comments(first: 5) {
          edges {
            node {
              content
              author
              replies(first: 3) {
                edges {
                  node {
                    content
                    author
                  }
                }
              }
            }
          }
        }
      }
    }
    pageInfo {
      hasNextPage
      endPosition
    }
  }
}
```

Создать комментарий:
```
mutation {
  createComment(input: {
    postId: "your-post-id"
    author: "Ivan"
    content: "Отличный пост!"
  }) {
    id
    content
    createdAt
  }
}
```

Подписка на новые комментарии:
```
subscription {
  commentAdded(postId: "your-post-id") {
    id
    author
    content
    createdAt
  }
}
```
Включить/отключить комментарии к посту:

```
mutation {
  toggleComments(postId: "your-post-id", allowComments: false) {
    id
    allowComments
  }
}
```
## Тесты

Все тесты:
go test ./test -v

Тесты с PostgreSQL:
docker compose up -d postgres
docker exec -it posts-service-postgres psql -U postgres -c "CREATE DATABASE postsdb_test;"
TEST_DB_URL="postgresql://postgres:password@localhost:5432/postsdb_test?sslmode=disable" go test ./test -v

Покрытие:
go test -cover ./...

---

## Как это работает

### Как сделать запрос

    1. HTTP запрос приходит на `/query`
    2. gqlgen парсит GraphQL операцию
    3. Middleware кладет DataLoader в контекст
    4. Резолвер достает DataLoader из контекста
    5. Резолвер вызывает метод хранилища (через DataLoader или напрямую)
    6. Хранилище возвращает данные

**Выбор хранилища** - при старте читается `STORAGE` (memory или postgres). Оба хранилища реализуют одинаковые интерфейсы, поэтому резолверы не знают, с каким работают.

### DataLoader и проблема N+1
Без DataLoader: запросили 10 постов - сделали 1 запрос. Для каждого поста нужно подгрузить комментарии - ещё 10 запросов. Итого 11 запросов.

С DataLoader: DataLoader копит ID постов в течение 2мс, потом делает один запрос за комментариями ко всем постам сразу. Итого 2 запроса.

Реализация: два лоадера в dataloader/dataloader.go:

CommentsByPostID - собирает ID постов

RepliesByParentID - собирает ID комментариев для загрузки ответов

Оба ждут 2мс, собирают ID и делают один запрос к БД через методы GetCommentsByPostIDs и GetRepliesByParentIDs.

### Подписки

commentAdded - уведомление о новом комментарии к посту. Реализовано по-разному в двух хранилищах:
In-memory: при вызове Subscribe создаётся канал, который кладётся в map[postID][]chan. Когда создаётся комментарий, он рассылается по всем каналам этого поста. 

использует LISTEN/NOTIFY. При создании комментария отправляется событие в канал post_{postID}. Отдельное соединение слушает события и отправляет их клиентам.


---

## Обзор файлов

### Точка входа

`server/main.go` - точка входа, вызывает run().

`server/app.go` - читает STORAGE, инициализирует хранилище, создаёт resolver, настраивает GraphQL-сервер с DataLoader middleware, запускает HTTP-сервер.

### GraphQL

`internal/graph/schema.graphqls` - GraphQL-схема. Определяет типы (Post, Comment, PostConnection, CommentEdge и др.), запросы (posts, post), мутации (createPost, createComment, toggleComments) и подписку (commentAdded).

`internal/graph/schema.resolvers.go` - реализация GraphQL-операций:
- createPost, posts, post -  для постов
- createComment, Comments, Replies - работа с комментариями, используют DataLoader
- commentAdded - подписка на новые комментарии через WebSocket
- toggleComments - включение/отключение комментариев у поста

`internal/graph/resolver.go` - структура Resolver, хранит зависимости (репозитории, лоадеры).

### Хранилища

`internal/repository/repository.go` - интерфейсы PostRepository и CommentRepository, позволяют иметь общий интерфейс для in-memory и PostgreSQL.

`internal/repository/memory.go` - in-memory реализация. Хранит данные в виде структуры из map с возможностью блокировки Подписки реализуются через каналы.

`internal/repository/postgres.go` - PostgreSQL-реализация через pgxpool.

### DataLoader

`dataloader/dataloader.go` - два лоадера для сощдания батчей запросов к БД: CommentsByPostID и RepliesByParentID.

`dataloader/context.go` - WithLoaders / GetLoaders для передачи лоадеров через context.Context.

