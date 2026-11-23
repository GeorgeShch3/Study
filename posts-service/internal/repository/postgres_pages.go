package repository

import (
	"context"
	"fmt"
	"posts-service/internal/model"
	"time"
)

func (s *PostgresStore) GetCommentPages(ctx context.Context, pages []CommentPage) ([]*model.CommentConnection, error) {
	return s.commentPages(ctx, pages, false)
}

func (s *PostgresStore) GetReplyPages(ctx context.Context, pages []CommentPage) ([]*model.CommentConnection, error) {
	return s.commentPages(ctx, pages, true)
}

func (s *PostgresStore) commentPages(ctx context.Context, pages []CommentPage, replies bool) ([]*model.CommentConnection, error) {
	result := make([]*model.CommentConnection, len(pages))
	if len(pages) == 0 {
		return result, nil
	}
	parents := make([]string, len(pages))
	limits := make([]int32, len(pages))
	cursors := make([]string, len(pages))
	for i, p := range pages {
		if err := p.validate(); err != nil {
			return nil, err
		}
		parents[i] = p.ParentID
		limits[i] = int32(p.First)
		cursors[i] = p.After
	}
	scope, order, comparison := "c.post_id = r.parent_id AND c.parent_id IS NULL", "DESC", "<"
	cursorScope := "cursor.post_id = r.parent_id AND cursor.parent_id IS NULL"
	if replies {
		scope, order, comparison = "c.parent_id = r.parent_id", "ASC", ">"
		cursorScope = "cursor.parent_id = r.parent_id"
	}
	query := fmt.Sprintf(`
 SELECT r.ordinal, totals.total_count,
        page.id::text, page.post_id::text, page.parent_id::text, page.author, page.content, page.created_at
 FROM unnest($1::uuid[], $2::int[], $3::text[]) WITH ORDINALITY AS r(parent_id, page_size, after_id, ordinal)
 CROSS JOIN LATERAL (
  SELECT count(*) AS total_count FROM comments c WHERE %s
 ) totals
 LEFT JOIN LATERAL (
  SELECT c.id, c.post_id, c.parent_id, c.author, c.content, c.created_at
  FROM comments c
  WHERE %s AND (r.after_id = '' OR (c.created_at, c.id) %s (
   SELECT cursor.created_at, cursor.id FROM comments cursor
   WHERE cursor.id = NULLIF(r.after_id, '')::uuid AND %s
  ))
  ORDER BY c.created_at %s, c.id %s
  LIMIT r.page_size + 1
 ) page ON TRUE
 ORDER BY r.ordinal, page.created_at %s, page.id %s`, scope, scope, comparison, cursorScope, order, order, order, order)
	rows, err := s.pool.Query(ctx, query, parents, limits, cursors)
	if err != nil {
		return nil, fmt.Errorf("get comment pages: %w", err)
	}
	defer rows.Close()
	comments := make([][]*model.Comment, len(pages))
	totals := make([]int, len(pages))
	for rows.Next() {
		var ordinal int
		var total int
		var id, postID, parentID, author, content *string
		var createdAt *time.Time
		if err := rows.Scan(&ordinal, &total, &id, &postID, &parentID, &author, &content, &createdAt); err != nil {
			return nil, fmt.Errorf("scan comment page: %w", err)
		}
		index := ordinal - 1
		totals[index] = total
		if id != nil {
			comments[index] = append(comments[index], &model.Comment{ID: *id, PostID: *postID, ParentID: parentID, Author: *author, Content: *content, CreatedAt: *createdAt})
		}
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("read comment pages: %w", err)
	}
	for i, p := range pages {
		result[i] = commentConnection(comments[i], p.First, totals[i])
	}
	return result, nil
}
