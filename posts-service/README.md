# Posts Service

A GraphQL service for posts and comments. Supports two storage backends (in-memory and PostgreSQL), real-time subscriptions, and query optimization via DataLoader.

## Requirements

Run commands below from the `posts-service` directory of the cloned repository.

- Native launch: Go 1.21+; PostgreSQL is needed only for the postgres storage mode.
- Container launch: Docker with the Compose plugin; local Go is not required.

## Running

In-memory, without a database:

```bash
go run ./server
```

Docker, in-memory:

```bash
docker compose --profile memory up --build
```

Docker, PostgreSQL:

```bash
docker compose --profile postgres up --build
```

Both modes publish the API and GraphQL playground at http://localhost:8080/.
Run one mode at a time. PostgreSQL uses local port 5432. If ports are already
occupied, set `APP_PORT` and `POSTGRES_PORT`, for example:

```bash
APP_PORT=18080 POSTGRES_PORT=15432 docker compose --profile postgres up --build
```

In this case use port 18080 in HTTP URLs and 15432 in native database URLs.
Use the same variables for later Compose commands.

Stop the selected mode:

```bash
docker compose --profile memory --profile postgres down
```

With a fresh Docker volume, PostgreSQL applies all SQL files in `migrations`
automatically. With an existing volume, apply new SQL files explicitly; the
current migrations are idempotent:

```bash
for migration in migrations/*.sql; do
  docker compose --profile postgres exec -T postgres \
    psql -U postgres -d postsdb -v ON_ERROR_STOP=1 < "$migration"
done
```

Native launch against an existing PostgreSQL:

```bash
export DATABASE_URL='postgresql://postgres:password@localhost:5432/postsdb?sslmode=disable'
for migration in migrations/*.sql; do
  psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -f "$migration"
done
STORAGE=postgres go run ./server
```

The database `postsdb` must already exist. The native migration commands require
`psql`. Credentials shown here are for local development.

## Environment Variables

| Variable | Default | Description |
|---|---|---|
| STORAGE | memory | Storage backend: memory or postgres |
| PORT | 8080 | HTTP server port |
| DATABASE_URL | postgresql://postgres@localhost:5432/postsdb?sslmode=disable | PostgreSQL connection string |
| TEST_DB_URL | — | Separate PostgreSQL database for integration tests |

## Example Queries

Create a post:
```graphql
mutation {
  createPost(title: "Hello", content: "First post", author: "Ivan") {
    id
    title
    createdAt
  }
}
```

Get a list of posts with comments:
```graphql
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

Create a comment:
```graphql
mutation {
  createComment(input: {
    postId: "your-post-id"
    author: "Ivan"
    content: "Great post!"
  }) {
    id
    content
    createdAt
  }
}
```

Subscribe to new comments:
```graphql
subscription {
  commentAdded(postId: "your-post-id") {
    id
    author
    content
    createdAt
  }
}
```

Enable/disable comments on a post:
```graphql
mutation {
  toggleComments(postId: "your-post-id", allowComments: false) {
    id
    allowComments
  }
}
```

## Tests

In-memory tests (including the GraphQL HTTP layer):

```bash
go test ./...
```

Tests with PostgreSQL, starting from a fresh clone and Docker installation:

```bash
docker compose --profile postgres up -d --wait postgres
docker compose --profile postgres exec -T postgres \
  psql -U postgres -v ON_ERROR_STOP=1 -c 'CREATE DATABASE postsdb_test;'
for migration in migrations/*.sql; do
  docker compose --profile postgres exec -T postgres \
    psql -U postgres -d postsdb_test -v ON_ERROR_STOP=1 < "$migration"
done
TEST_DB_URL='postgresql://postgres:password@localhost:5432/postsdb_test?sslmode=disable' go test -count=1 ./...
```

Create `postsdb_test` only once. Tests truncate data; do not point `TEST_DB_URL`
at an application database.

Race detection and static checks:

```bash
go test -race -count=1 ./...
go vet ./...
```

The PostgreSQL pagination regression test seeds 1,000 comments per parent and
checks that a batch of two pages of size 10 transfers only 22 rows from the DB
(including one lookahead row per page). It also checks deterministic ordering
when timestamps are equal. Other tests cover independent cursors and limits,
empty pages, GraphQL aliases, and cache isolation between requests.

## How It Works

### How a request is processed

1. An HTTP request arrives at `/query`
2. gqlgen parses the GraphQL operation
3. Middleware places the DataLoader into the context
4. The resolver retrieves the DataLoader from the context
5. The resolver calls a storage method (via the DataLoader or directly)
6. The storage returns the data

**Storage selection** — at startup, `STORAGE` is read (memory or postgres). Both backends implement the same interfaces, so resolvers don't know which one they're working with.

### DataLoader and pagination

Loaders are created for each GraphQL operation. A cache key contains the parent
ID, `first`, and `after`, so aliases requesting different pages do not share the
wrong cached result. No cache is shared between HTTP requests.

The loaders wait 2 ms to batch concurrent loads. PostgreSQL receives the page
keys in one query using `unnest` and `LATERAL`. For each parent it returns at most
`first + 1` comments, rather than loading every comment and slicing in Go. The
extra row determines `hasNextPage`; `totalCount` is counted separately inside
the same SQL statement. Exact counts still require counting matching rows.

Root comments are ordered by `(created_at, id)` descending; replies use ascending
order. Cursors must belong to the same post or thread. Unknown or foreign
cursors return an empty page with the parent's `totalCount`. Malformed UUID
cursors produce an error. GraphQL `first` defaults to 10 and accepts 1–100 for
posts, comments, and replies.

Composite indexes are defined in `migrations/002_comment_pagination.sql`.

### Subscriptions

`commentAdded` — a notification about a new comment on a post. Implemented differently in the two backends:

- **In-memory:** when `Subscribe` is called, a channel is created and placed into a `map[postID][]chan`. When a comment is created, it is broadcast to all channels for that post.
- **PostgreSQL:** uses `LISTEN/NOTIFY`. When a comment is created, an event is sent to the channel `post_{postID}`. A separate connection listens for events and forwards them to clients.

---

## File Overview

### Entry point

- `server/main.go` — entry point, calls `run()`.
- `server/app.go` — reads `STORAGE`, initializes the storage backend, creates the resolver, configures the GraphQL server with DataLoader middleware, and starts the HTTP server.

### GraphQL

- `internal/graph/schema.graphqls` — the GraphQL schema. Defines types (Post, Comment, PostConnection, CommentEdge, etc.), queries (posts, post), mutations (createPost, createComment, toggleComments), and a subscription (commentAdded).
- `internal/graph/schema.resolvers.go` — implementation of the GraphQL operations:
  - createPost, posts, post — for posts
  - createComment, Comments, Replies — handling comments, using DataLoader
  - commentAdded — subscription to new comments via WebSocket
  - toggleComments — enabling/disabling comments on a post
- `internal/graph/resolver.go` — the Resolver struct, holds dependencies (repositories, loaders).

### Storage backends

- `internal/repository/repository.go` — the PostRepository and CommentRepository interfaces, providing a common interface for in-memory and PostgreSQL.
- `internal/repository/memory.go` — in-memory implementation. Stores data as a struct of maps with locking support. Subscriptions are implemented via channels.
- `internal/repository/postgres.go` — PostgreSQL implementation via pgxpool.

### DataLoader

- `dataloader/dataloader.go` — two loaders for batching database queries: CommentsByPostID and RepliesByParentID.
- `dataloader/context.go` — WithLoaders / GetLoaders for passing loaders through context.Context.
