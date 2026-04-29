package repository

import (
	"context"
	"posts-service/internal/model"
	"sort"
	"strconv"
	"sync"
	"time"

	"github.com/google/uuid"
)

type ErrPostExists struct{ PostID string }

func (e *ErrPostExists) Error() string { return "post already exists: " + e.PostID }

type ErrPostNotFound struct{ PostID string }

func (e *ErrPostNotFound) Error() string { return "post not found: " + e.PostID }

type ErrCommentTooLong struct{ MaxLength int }

func (e *ErrCommentTooLong) Error() string {
	return "comment too long: max " + strconv.Itoa(e.MaxLength) + " characters"
}

type ErrCommentsDisabled struct{ PostID string }

func (e *ErrCommentsDisabled) Error() string {
	return "comments are disabled for this post: " + e.PostID
}

type MemoryStore struct {
	mu          sync.RWMutex
	posts       map[string]*model.Post
	comments    map[string]*model.Comment
	postOrder   []string            
	postRoots   map[string][]string 
	byParent    map[string][]string 
	subscribers map[string][]chan *model.Comment
}

func NewMemoryStore() *MemoryStore {
	return &MemoryStore{
		posts:       make(map[string]*model.Post),
		comments:    make(map[string]*model.Comment),
		postOrder:   []string{},
		postRoots:   make(map[string][]string),
		byParent:    make(map[string][]string),
		subscribers: make(map[string][]chan *model.Comment),
	}
}

func (s *MemoryStore) CreatePost(ctx context.Context, post *model.Post) error {
	if err := checkContext(ctx); err != nil {
		return err
	}

	s.mu.Lock()
	defer s.mu.Unlock()

	if post.ID == "" {
		post.ID = generateID()
	} else if _, exists := s.posts[post.ID]; exists {
		return &ErrPostExists{PostID: post.ID}
	}

	post.AllowComments = true
	post.CreatedAt = time.Now()

	clone := *post
	s.posts[post.ID] = &clone
	s.postOrder = append([]string{post.ID}, s.postOrder...)
	*post = clone
	return nil
}

func (s *MemoryStore) GetPostByID(ctx context.Context, id string) (*model.Post, error) {
	if err := checkContext(ctx); err != nil {
		return nil, err
	}

	s.mu.RLock()
	defer s.mu.RUnlock()

	post, ok := s.posts[id]
	if !ok {
		return nil, &ErrPostNotFound{PostID: id}
	}
	clone := *post
	return &clone, nil
}

func (s *MemoryStore) GetPosts(ctx context.Context, first int, after *string) (*model.PostConnection, error) {
	if err := checkContext(ctx); err != nil {
		return nil, err
	}
	if first <= 0 {
		first = 10
	}

	s.mu.RLock()
	defer s.mu.RUnlock()

	order := s.postOrder
	totalCount := len(order)

	start := 0
	if after != nil && *after != "" {
		found := false
		for i, id := range order {
			if id == *after {
				start = i + 1
				found = true
				break
			}
		}
		if !found {
			start = 0
		}
	}

	end := start + first
	if end > len(order) {
		end = len(order)
	}

	pageIDs := order[start:end]
	edges := make([]*model.PostEdge, 0, len(pageIDs))
	for _, id := range pageIDs {
		if p, ok := s.posts[id]; ok {
			clone := *p
			edges = append(edges, &model.PostEdge{Node: &clone, Position: id})
		}
	}

	hasNextPage := end < totalCount
	var endPosition *string
	if len(edges) > 0 {
		endPosition = &edges[len(edges)-1].Position
	}

	return &model.PostConnection{
		Edges:      edges,
		PageInfo:   &model.PageInfo{HasNextPage: hasNextPage, EndPosition: endPosition},
		TotalCount: totalCount,
	}, nil
}

func (s *MemoryStore) ToggleComments(ctx context.Context, postID string, allowComments bool) (*model.Post, error) {
	if err := checkContext(ctx); err != nil {
		return nil, err
	}

	s.mu.Lock()
	defer s.mu.Unlock()

	post, ok := s.posts[postID]
	if !ok {
		return nil, &ErrPostNotFound{PostID: postID}
	}
	post.AllowComments = allowComments
	clone := *post
	return &clone, nil
}

func (s *MemoryStore) CreateComment(ctx context.Context, comment *model.Comment) (*model.Comment, error) {
	if err := checkContext(ctx); err != nil {
		return nil, err
	}
	if len(comment.Content) > 2000 {
		return nil, &ErrCommentTooLong{MaxLength: 2000}
	}

	s.mu.Lock()

	post, ok := s.posts[comment.PostID]
	if !ok {
		s.mu.Unlock()
		return nil, &ErrPostNotFound{PostID: comment.PostID}
	}
	if !post.AllowComments {
		s.mu.Unlock()
		return nil, &ErrCommentsDisabled{PostID: comment.PostID}
	}

	comment.ID = generateID()
	comment.CreatedAt = time.Now()

	clone := *comment
	s.comments[clone.ID] = &clone

	if clone.ParentID == nil {
		s.postRoots[clone.PostID] = append(s.postRoots[clone.PostID], clone.ID)
	} else {
		s.byParent[*clone.ParentID] = append(s.byParent[*clone.ParentID], clone.ID)
	}

	subs := make([]chan *model.Comment, len(s.subscribers[clone.PostID]))
	copy(subs, s.subscribers[clone.PostID])
	s.mu.Unlock()

	result := clone
	for _, ch := range subs {
		select {
		case ch <- &result:
		default: 
		}
	}

	*comment = clone
	return &result, nil
}

func (s *MemoryStore) GetCommentsByPostID(ctx context.Context, postID string, first int, after *string) (*model.CommentConnection, error) {
	if err := checkContext(ctx); err != nil {
		return nil, err
	}
	if first <= 0 {
		first = 10
	}

	s.mu.RLock()
	defer s.mu.RUnlock()

	rootIDs := s.postRoots[postID]
	if len(rootIDs) == 0 {
		return emptyCommentConnection(), nil
	}

	reversed := reverseStrings(rootIDs)
	return s.paginateCommentIDs(reversed, first, after)
}

func (s *MemoryStore) GetRepliesByParentID(ctx context.Context, parentID string, first int, after *string) (*model.CommentConnection, error) {
	if err := checkContext(ctx); err != nil {
		return nil, err
	}
	if first <= 0 {
		first = 10
	}

	s.mu.RLock()
	defer s.mu.RUnlock()

	replyIDs := s.byParent[parentID]
	if len(replyIDs) == 0 {
		return emptyCommentConnection(), nil
	}

	return s.paginateCommentIDs(replyIDs, first, after)
}

func (s *MemoryStore) GetCommentsByPostIDs(ctx context.Context, postIDs []string) (map[string][]*model.Comment, error) {
	if err := checkContext(ctx); err != nil {
		return nil, err
	}

	s.mu.RLock()
	defer s.mu.RUnlock()

	result := make(map[string][]*model.Comment)

	for _, postID := range postIDs {
		rootIDs := s.postRoots[postID]
		comments := make([]*model.Comment, 0, len(rootIDs))

		for _, commentID := range rootIDs {
			if c, ok := s.comments[commentID]; ok {
				clone := *c
				comments = append(comments, &clone)
			}
		}

		sort.Slice(comments, func(i, j int) bool {
			return comments[i].CreatedAt.After(comments[j].CreatedAt)
		})

		result[postID] = comments
	}

	return result, nil
}

func (s *MemoryStore) Subscribe(ctx context.Context, postID string) (<-chan *model.Comment, error) {
	ch := make(chan *model.Comment, 16)

	s.mu.Lock()
	s.subscribers[postID] = append(s.subscribers[postID], ch)
	s.mu.Unlock()

	go func() {
		<-ctx.Done()
		s.mu.Lock()
		defer s.mu.Unlock()

		subs := s.subscribers[postID]
		for i, sub := range subs {
			if sub == ch {
				s.subscribers[postID] = append(subs[:i], subs[i+1:]...)
				break
			}
		}
		close(ch)
	}()

	return ch, nil
}

func generateID() string { return uuid.New().String() }

func checkContext(ctx context.Context) error {
	select {
	case <-ctx.Done():
		return ctx.Err()
	default:
		return nil
	}
}

func emptyCommentConnection() *model.CommentConnection {
	return &model.CommentConnection{
		Edges:    []*model.CommentEdge{},
		PageInfo: &model.PageInfo{HasNextPage: false},
	}
}

func reverseStrings(s []string) []string {
	n := len(s)
	out := make([]string, n)
	for i, v := range s {
		out[n-1-i] = v
	}
	return out
}

func (s *MemoryStore) paginateCommentIDs(
	ids []string,
	first int,
	after *string,
) (*model.CommentConnection, error) {
	totalCount := len(ids)

	start := 0
	if after != nil && *after != "" {
		for i, id := range ids {
			if id == *after {
				start = i + 1
				break
			}
		}
	}

	end := start + first
	if end > len(ids) {
		end = len(ids)
	}

	pageIDs := ids[start:end]
	edges := make([]*model.CommentEdge, 0, len(pageIDs))
	for _, id := range pageIDs {
		if c, ok := s.comments[id]; ok {
			clone := *c
			edges = append(edges, &model.CommentEdge{Node: &clone, Position: id})
		}
	}

	hasNextPage := end < totalCount
	var endPosition *string
	if len(edges) > 0 {
		endPosition = &edges[len(edges)-1].Position
	}

	return &model.CommentConnection{
		Edges:      edges,
		PageInfo:   &model.PageInfo{HasNextPage: hasNextPage, EndPosition: endPosition},
		TotalCount: totalCount,
	}, nil
}

func (s *MemoryStore) GetRepliesByParentIDs(ctx context.Context, parentIDs []string) (map[string][]*model.Comment, error) {
	if err := checkContext(ctx); err != nil {
		return nil, err
	}

	s.mu.RLock()
	defer s.mu.RUnlock()

	result := make(map[string][]*model.Comment)

	for _, parentID := range parentIDs {
		replyIDs := s.byParent[parentID]
		replies := make([]*model.Comment, 0, len(replyIDs))

		for _, replyID := range replyIDs {
			if c, ok := s.comments[replyID]; ok {
				clone := *c
				replies = append(replies, &clone)
			}
		}

		sort.Slice(replies, func(i, j int) bool {
			return replies[i].CreatedAt.Before(replies[j].CreatedAt)
		})

		result[parentID] = replies
	}

	return result, nil
}

func (s *MemoryStore) Close() error {
    return nil
}