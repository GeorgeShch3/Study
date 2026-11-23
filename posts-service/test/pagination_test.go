package repository_test

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"sync/atomic"
	"testing"

	"github.com/99designs/gqlgen/graphql"
	"github.com/99designs/gqlgen/graphql/handler"
	"github.com/google/uuid"
	"posts-service/dataloader"
	"posts-service/internal/graph"
	"posts-service/internal/model"
	"posts-service/internal/repository"
)

func TestCommentPagesKeepIndependentLimitsAndCursors(t *testing.T) {
	truncate(t)
	s := newStore()
	ctx := context.Background()
	p := mustCreatePost(t, s, "T", "C", "A")
	other := mustCreatePost(t, s, "Other", "C", "A")
	roots := make([]*model.Comment, 12)
	for i := range roots {
		roots[i] = mustCreateComment(t, s, p.ID, nil, "A", fmt.Sprint(i))
	}
	foreign := mustCreateComment(t, s, other.ID, nil, "A", "Foreign")
	pages, err := s.GetCommentPages(ctx, []repository.CommentPage{
		{ParentID: p.ID, First: 2}, {ParentID: p.ID, First: 4, After: roots[10].ID},
		{ParentID: other.ID, First: 3}, {ParentID: p.ID, First: 3, After: foreign.ID},
		{ParentID: p.ID, First: 3, After: uuid.NewString()},
	})
	if err != nil {
		t.Fatal(err)
	}
	if len(pages) != 5 {
		t.Fatalf("pages=%d", len(pages))
	}
	if len(pages[0].Edges) != 2 || pages[0].TotalCount != 12 || !pages[0].PageInfo.HasNextPage {
		t.Fatalf("first page: %+v", pages[0])
	}
	if pages[0].Edges[0].Node.ID != roots[11].ID {
		t.Fatal("root comments must be newest first")
	}
	if len(pages[1].Edges) != 4 || pages[1].Edges[0].Node.ID != roots[9].ID {
		t.Fatal("cursor/limit was not applied independently")
	}
	if len(pages[2].Edges) != 1 || pages[2].PageInfo.HasNextPage {
		t.Fatal("single-comment page")
	}
	for _, page := range pages[3:] {
		if len(page.Edges) != 0 || page.TotalCount != 12 || page.PageInfo.HasNextPage {
			t.Fatal("unknown/foreign cursor must return an empty page with its total count")
		}
	}
	last, err := s.GetCommentsByPostID(ctx, p.ID, 2, &roots[0].ID)
	if err != nil || len(last.Edges) != 0 || last.TotalCount != 12 || last.PageInfo.EndPosition != nil {
		t.Fatalf("last page: %v, %v", last, err)
	}
}

func TestReplyPagesAreScopedAndOldestFirst(t *testing.T) {
	truncate(t)
	s := newStore()
	ctx := context.Background()
	p := mustCreatePost(t, s, "T", "C", "A")
	root := mustCreateComment(t, s, p.ID, nil, "A", "Root")
	other := mustCreateComment(t, s, p.ID, nil, "A", "Other")
	replies := make([]*model.Comment, 6)
	for i := range replies {
		replies[i] = mustCreateComment(t, s, p.ID, &root.ID, "A", fmt.Sprint(i))
	}
	foreign := mustCreateComment(t, s, p.ID, &other.ID, "A", "Foreign reply")
	pages, err := s.GetReplyPages(ctx, []repository.CommentPage{
		{ParentID: root.ID, First: 2}, {ParentID: root.ID, First: 3, After: replies[1].ID},
		{ParentID: root.ID, First: 3, After: foreign.ID}, {ParentID: foreign.ID, First: 2},
	})
	if err != nil {
		t.Fatal(err)
	}
	if len(pages[0].Edges) != 2 || pages[0].Edges[0].Node.ID != replies[0].ID || pages[0].TotalCount != 6 {
		t.Fatal("reply order/count")
	}
	if len(pages[1].Edges) != 3 || pages[1].Edges[0].Node.ID != replies[2].ID || !pages[1].PageInfo.HasNextPage {
		t.Fatal("reply cursor/limit")
	}
	if len(pages[2].Edges) != 0 || pages[2].TotalCount != 6 {
		t.Fatal("foreign reply cursor")
	}
	if len(pages[3].Edges) != 0 || pages[3].TotalCount != 0 {
		t.Fatal("empty thread")
	}
}

type countedComments struct {
	repository.CommentRepository
	calls atomic.Int64
}

func (s *countedComments) GetCommentPages(ctx context.Context, keys []repository.CommentPage) ([]*model.CommentConnection, error) {
	s.calls.Add(1)
	return s.CommentRepository.GetCommentPages(ctx, keys)
}
func TestDataLoaderBatchesPagesWithoutMixingKeys(t *testing.T) {
	truncate(t)
	s := newStore()
	ctx := context.Background()
	p := mustCreatePost(t, s, "T", "C", "A")
	roots := make([]*model.Comment, 5)
	for i := range roots {
		roots[i] = mustCreateComment(t, s, p.ID, nil, "A", "C")
	}
	counted := &countedComments{CommentRepository: s}
	loader := dataloader.NewLoaders(counted)
	pages, err := loader.CommentsByPostID.LoadAll(ctx, []repository.CommentPage{
		{ParentID: p.ID, First: 1}, {ParentID: p.ID, First: 3}, {ParentID: p.ID, First: 2, After: roots[3].ID},
	})
	if err != nil {
		t.Fatal(err)
	}
	if counted.calls.Load() != 1 {
		t.Fatalf("want one batch, got %d", counted.calls.Load())
	}
	if len(pages[0].Edges) != 1 || len(pages[1].Edges) != 3 || len(pages[2].Edges) != 2 || pages[2].Edges[0].Node.ID != roots[2].ID {
		t.Fatal("page keys collided")
	}
}

func TestGraphQLPaginationAndCacheAreRequestScoped(t *testing.T) {
	truncate(t)
	s := newStore()
	p := mustCreatePost(t, s, "T", "C", "A")
	for i := 0; i < 4; i++ {
		mustCreateComment(t, s, p.ID, nil, "A", "C")
	}
	resolver := graph.NewResolver(s, s)
	server := handler.NewDefaultServer(graph.NewExecutableSchema(graph.Config{Resolvers: resolver}))
	server.AroundOperations(func(ctx context.Context, next graphql.OperationHandler) graphql.ResponseHandler {
		return next(resolver.DataLoaderMiddleware(ctx))
	})
	query := fmt.Sprintf(`{post(id:%q){small:comments(first:1){edges{node{id}} totalCount} large:comments(first:3){edges{node{id}}}}}`, p.ID)
	send := func(query string) map[string]any {
		t.Helper()
		payload, _ := json.Marshal(map[string]string{"query": query})
		request := httptest.NewRequest(http.MethodPost, "/query", bytes.NewReader(payload))
		request.Header.Set("Content-Type", "application/json")
		response := httptest.NewRecorder()
		server.ServeHTTP(response, request)
		var result map[string]any
		if err := json.Unmarshal(response.Body.Bytes(), &result); err != nil {
			t.Fatal(err)
		}
		return result
	}
	first := send(query)
	if first["errors"] != nil {
		t.Fatal(first)
	}
	post := first["data"].(map[string]any)["post"].(map[string]any)
	if len(post["small"].(map[string]any)["edges"].([]any)) != 1 || len(post["large"].(map[string]any)["edges"].([]any)) != 3 {
		t.Fatal("GraphQL aliases share the wrong page")
	}
	newest := mustCreateComment(t, s, p.ID, nil, "A", "New")
	next := send(query)
	small := next["data"].(map[string]any)["post"].(map[string]any)["small"].(map[string]any)
	node := small["edges"].([]any)[0].(map[string]any)["node"].(map[string]any)
	if node["id"] != newest.ID || small["totalCount"] != float64(5) {
		t.Fatal("stale cache crossed requests")
	}
	for _, size := range []int{-1, 0, 101, 2147483647} {
		result := send(fmt.Sprintf(`{post(id:%q){comments(first:%d){totalCount}}}`, p.ID, size))
		if result["errors"] == nil {
			t.Fatalf("accepted first=%d", size)
		}
	}
}
