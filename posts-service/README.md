# Posts Service

A GraphQL service for posts and comments. Supports two storage backends (in-memory and PostgreSQL), real-time subscriptions, and query optimization via DataLoader.

## Requirements

- Go 1.21+
- Docker
- PostgreSQL

## Environment Variables

| Variable | Default | Description |
|---|---|---|
| STORAGE | memory | Storage backend: memory or postgres |
| PORT | 8080 | HTTP server port |
| DATABASE_URL | postgresql://postgres@localhost:5432/postsdb?sslmode=disable | PostgreSQL connection string |
| TEST_DB_URL | — | Connection string for integration tests |

## Running

In-memory:
`go run server/*.go`

PostgreSQL:
`STORAGE=postgres DATABASE_URL="postgresql://postgres:password@localhost:5432/postsdb?sslmode=disable" go run server/*.go`

Docker in-memory:
`docker compose --profile memory up --build`

Docker PostgreSQL:
`docker compose --profile postgres up --build`

Stop Docker:
`docker compose down`

Once running, the GraphQL playground is available at http://localhost:8080/.

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

All tests:
```bash
go test ./test -v
```

Tests with PostgreSQL:
```bash
docker compose up -d postgres
docker exec -it posts-service-postgres psql -U postgres -c "CREATE DATABASE postsdb_test;"
TEST_DB_URL="postgresql://postgres:password@localhost:5432/postsdb_test?sslmode=disable" go test ./test -v
```

Coverage:
```bash
go test -cover ./...
```

---

## How It Works

### How a request is processed

1. An HTTP request arrives at `/query`
2. gqlgen parses the GraphQL operation
3. Middleware places the DataLoader into the context
4. The resolver retrieves the DataLoader from the context
5. The resolver calls a storage method (via the DataLoader or directly)
6. The storage returns the data

**Storage selection** — at startup, `STORAGE` is read (memory or postgres). Both backends implement the same interfaces, so resolvers don't know which one they're working with.

### DataLoader and the N+1 problem

Without DataLoader: you request 10 posts — that's 1 query. For each post you need to load its comments — that's 10 more queries. 11 queries total.

With DataLoader: the DataLoader accumulates post IDs over 2ms, then makes a single query for the comments of all posts at once. 2 queries total.

Implementation: two loaders in `dataloader/dataloader.go`:

- `CommentsByPostID` — collects post IDs
- `RepliesByParentID` — collects comment IDs to load replies

Both wait 2ms, gather the IDs, and make a single query to the database via the methods `GetCommentsByPostIDs` and `GetRepliesByParentIDs`.

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
