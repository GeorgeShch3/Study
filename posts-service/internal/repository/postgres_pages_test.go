package repository

import (
	"context"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v4"
	"github.com/jackc/pgx/v4/pgxpool"
	"posts-service/internal/model"
)

type pageQueryLog struct {
	mu     sync.Mutex
	counts []int
}

func (l *pageQueryLog) Log(_ context.Context, _ pgx.LogLevel, message string, data map[string]interface{}) {
	if message != "Query" {
		return
	}
	sql, _ := data["sql"].(string)
	if !strings.Contains(sql, "WITH ORDINALITY") {
		return
	}
	count, ok := data["rowCount"].(int)
	if ok {
		l.mu.Lock()
		l.counts = append(l.counts, count)
		l.mu.Unlock()
	}
}

func TestPostgresTransfersOnlyRequestedPageRows(t *testing.T) {
	dbURL := os.Getenv("TEST_DB_URL")
	if dbURL == "" {
		t.Skip("TEST_DB_URL is not configured")
	}
	ctx := context.Background()
	admin, err := pgx.Connect(ctx, dbURL)
	if err != nil {
		t.Fatal(err)
	}
	defer admin.Close(ctx)
	schema := "pagination_" + strings.ReplaceAll(uuid.NewString(), "-", "")
	if _, err := admin.Exec(ctx, "CREATE SCHEMA "+schema); err != nil {
		t.Fatal(err)
	}
	defer admin.Exec(ctx, "DROP SCHEMA "+schema+" CASCADE")
	config, err := pgxpool.ParseConfig(dbURL)
	if err != nil {
		t.Fatal(err)
	}
	log := &pageQueryLog{}
	config.ConnConfig.RuntimeParams["search_path"] = schema
	config.ConnConfig.Logger = log
	config.ConnConfig.LogLevel = pgx.LogLevelInfo
	pool, err := pgxpool.ConnectConfig(ctx, config)
	if err != nil {
		t.Fatal(err)
	}
	defer pool.Close()
	migrations, err := filepath.Glob("../../migrations/*.sql")
	if err != nil {
		t.Fatal(err)
	}
	for _, name := range migrations {
		sql, err := os.ReadFile(name)
		if err != nil {
			t.Fatal(err)
		}
		if _, err := pool.Exec(ctx, string(sql)); err != nil {
			t.Fatal(err)
		}
	}
	store := &PostgresStore{pool: pool, connString: dbURL}
	keys := make([]CommentPage, 2)
	for i := range keys {
		post := &model.Post{Title: "T", Content: "C", Author: "A"}
		if err := store.CreatePost(ctx, post); err != nil {
			t.Fatal(err)
		}
		_, err := pool.Exec(ctx, `INSERT INTO comments(post_id,author,content,created_at)
   SELECT $1,'A',repeat('x',2000),'2026-01-01'::timestamptz FROM generate_series(1,1000)`, post.ID)
		if err != nil {
			t.Fatal(err)
		}
		keys[i] = CommentPage{ParentID: post.ID, First: 10}
	}
	pages, err := store.GetCommentPages(ctx, keys)
	if err != nil {
		t.Fatal(err)
	}
	for _, page := range pages {
		if len(page.Edges) != 10 || page.TotalCount != 1000 || !page.PageInfo.HasNextPage {
			t.Fatalf("bad page: %+v", page)
		}
		for i := 1; i < len(page.Edges); i++ {
			if page.Edges[i-1].Node.ID <= page.Edges[i].Node.ID {
				t.Fatal("timestamp ties must be broken by ID")
			}
		}
	}
	if len(log.counts) != 1 || log.counts[0] != 22 {
		t.Fatalf("want one SQL query returning 22 rows, got %v", log.counts)
	}
}
