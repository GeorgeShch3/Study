package repository

import (
	"context"
	"posts-service/internal/model"
)

func (s *MemoryStore) GetCommentPages(ctx context.Context, pages []CommentPage) ([]*model.CommentConnection, error) {
	return s.commentPages(ctx, pages, false)
}

func (s *MemoryStore) GetReplyPages(ctx context.Context, pages []CommentPage) ([]*model.CommentConnection, error) {
	return s.commentPages(ctx, pages, true)
}

func (s *MemoryStore) commentPages(ctx context.Context, pages []CommentPage, replies bool) ([]*model.CommentConnection, error) {
	if err := checkContext(ctx); err != nil {
		return nil, err
	}
	for _, page := range pages {
		if err := page.validate(); err != nil {
			return nil, err
		}
	}
	s.mu.RLock()
	defer s.mu.RUnlock()
	result := make([]*model.CommentConnection, len(pages))
	for i, p := range pages {
		ids := s.postRoots[p.ParentID]
		if replies {
			ids = s.byParent[p.ParentID]
		} else {
			ids = reverseStrings(ids)
		}
		var after *string
		if p.After != "" {
			after = &p.After
		}
		connection, err := s.paginateCommentIDs(ids, p.First, after)
		if err != nil {
			return nil, err
		}
		result[i] = connection
	}
	return result, nil
}
