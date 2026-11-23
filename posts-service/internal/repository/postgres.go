package repository

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"posts-service/internal/model"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v4"
	"github.com/jackc/pgx/v4/pgxpool"
)

type PostgresStore struct {
	pool       *pgxpool.Pool
	connString string
}

func NewPostgresStore(connString string) (*PostgresStore, error) {
	config, err := pgxpool.ParseConfig(connString)
	if err != nil {
		return nil, fmt.Errorf("failed to parse config: %w", err)
	}

	pool, err := pgxpool.ConnectConfig(context.Background(), config)
	if err != nil {
		return nil, fmt.Errorf("failed to connect to database: %w", err)
	}

	if err := pool.Ping(context.Background()); err != nil {
		pool.Close()
		return nil, fmt.Errorf("failed to ping database: %w", err)
	}

	return &PostgresStore{pool: pool, connString: connString}, nil
}

func (s *PostgresStore) Close() {
	s.pool.Close()
}

func (s *PostgresStore) CreatePost(ctx context.Context, post *model.Post) error {
	if post.ID == "" {
		post.ID = generateID()
	}
	post.AllowComments = true
	post.CreatedAt = time.Now()

	const q = `
		INSERT INTO posts (id, title, content, author, allow_comments, created_at)
		VALUES ($1, $2, $3, $4, $5, $6)`

	_, err := s.pool.Exec(ctx, q,
		post.ID, post.Title, post.Content, post.Author, post.AllowComments, post.CreatedAt)
	if err != nil {
		return fmt.Errorf("failed to create post: %w", err)
	}
	return nil
}

func (s *PostgresStore) GetPostByID(ctx context.Context, id string) (*model.Post, error) {
	if _, err := uuid.Parse(id); err != nil {
		return nil, &ErrPostNotFound{PostID: id}
	}
	const q = `SELECT id, title, content, author, allow_comments, created_at FROM posts WHERE id = $1`

	var p model.Post
	err := s.pool.QueryRow(ctx, q, id).Scan(
		&p.ID, &p.Title, &p.Content, &p.Author, &p.AllowComments, &p.CreatedAt)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, &ErrPostNotFound{PostID: id}
		}
		return nil, fmt.Errorf("failed to get post: %w", err)
	}
	return &p, nil
}

func (s *PostgresStore) GetPosts(ctx context.Context, first int, after *string) (*model.PostConnection, error) {
	if first <= 0 {
		first = 10
	}

	var totalCount int
	if err := s.pool.QueryRow(ctx, "SELECT COUNT(*) FROM posts").Scan(&totalCount); err != nil {
		return nil, fmt.Errorf("failed to count posts: %w", err)
	}

	var rows pgx.Rows
	var err error

	if after == nil || *after == "" {
		rows, err = s.pool.Query(ctx,
			`SELECT id, title, content, author, allow_comments, created_at
			 FROM posts ORDER BY created_at DESC, id DESC LIMIT $1`,
			first+1)
	} else {
		rows, err = s.pool.Query(ctx,
			`SELECT id, title, content, author, allow_comments, created_at
			 FROM posts
			 WHERE (created_at, id) < (
			     SELECT created_at, id FROM posts WHERE id = $1
			 )
			 ORDER BY created_at DESC, id DESC LIMIT $2`,
			*after, first+1)
	}
	if err != nil {
		return nil, fmt.Errorf("failed to get posts: %w", err)
	}
	defer rows.Close()

	var posts []*model.Post
	for rows.Next() {
		p := &model.Post{}
		if err := rows.Scan(&p.ID, &p.Title, &p.Content, &p.Author, &p.AllowComments, &p.CreatedAt); err != nil {
			return nil, fmt.Errorf("failed to scan post: %w", err)
		}
		posts = append(posts, p)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("rows error: %w", err)
	}

	hasNextPage := len(posts) > first
	if hasNextPage {
		posts = posts[:first]
	}

	edges := make([]*model.PostEdge, len(posts))
	for i, p := range posts {
		edges[i] = &model.PostEdge{Node: p, Position: p.ID}
	}

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

func (s *PostgresStore) ToggleComments(ctx context.Context, postID string, allowComments bool) (*model.Post, error) {
	if _, err := uuid.Parse(postID); err != nil {
		return nil, &ErrPostNotFound{PostID: postID}
	}
	const q = `
		UPDATE posts SET allow_comments = $1 WHERE id = $2
		RETURNING id, title, content, author, allow_comments, created_at`

	var p model.Post
	err := s.pool.QueryRow(ctx, q, allowComments, postID).Scan(
		&p.ID, &p.Title, &p.Content, &p.Author, &p.AllowComments, &p.CreatedAt)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, &ErrPostNotFound{PostID: postID}
		}
		return nil, fmt.Errorf("failed to toggle comments: %w", err)
	}
	return &p, nil
}

func (s *PostgresStore) CreateComment(ctx context.Context, comment *model.Comment) (*model.Comment, error) {
	if _, err := uuid.Parse(comment.PostID); err != nil {
		return nil, &ErrPostNotFound{PostID: comment.PostID}
	}
	if len(comment.Content) > 2000 {
		return nil, &ErrCommentTooLong{MaxLength: 2000}
	}

	var allowComments bool
	err := s.pool.QueryRow(ctx, "SELECT allow_comments FROM posts WHERE id = $1", comment.PostID).Scan(&allowComments)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, &ErrPostNotFound{PostID: comment.PostID}
		}
		return nil, fmt.Errorf("failed to check post: %w", err)
	}
	if !allowComments {
		return nil, &ErrCommentsDisabled{PostID: comment.PostID}
	}

	if comment.ParentID != nil {
		var exists bool
		if err := s.pool.QueryRow(ctx,
			"SELECT EXISTS(SELECT 1 FROM comments WHERE id = $1)", *comment.ParentID,
		).Scan(&exists); err != nil {
			return nil, fmt.Errorf("failed to check parent: %w", err)
		}
		if !exists {
			return nil, fmt.Errorf("parent comment not found: %s", *comment.ParentID)
		}
	}

	if comment.ID == "" {
		comment.ID = generateID()
	}
	comment.CreatedAt = time.Now()

	const q = `
		INSERT INTO comments (id, post_id, parent_id, author, content, created_at)
		VALUES ($1, $2, $3, $4, $5, $6)
		RETURNING id, post_id, parent_id, author, content, created_at`

	result := &model.Comment{}
	err = s.pool.QueryRow(ctx, q,
		comment.ID, comment.PostID, comment.ParentID, comment.Author, comment.Content, comment.CreatedAt,
	).Scan(&result.ID, &result.PostID, &result.ParentID, &result.Author, &result.Content, &result.CreatedAt)
	if err != nil {
		return nil, fmt.Errorf("failed to create comment: %w", err)
	}

	go s.notify(result)

	return result, nil
}

func (s *PostgresStore) notify(comment *model.Comment) {
	payload, err := json.Marshal(comment)
	if err != nil {
		return
	}
	channelName := "post_" + comment.PostID

	ctx := context.Background()
	conn, err := s.pool.Acquire(ctx)
	if err != nil {
		return
	}
	defer conn.Release()

	safePayload := string(payload)
	_, _ = conn.Exec(ctx, "SELECT pg_notify($1, $2)", channelName, safePayload)
}

func (s *PostgresStore) GetCommentsByPostID(ctx context.Context, postID string, first int, after *string) (*model.CommentConnection, error) {
	pages, err := s.GetCommentPages(ctx, []CommentPage{NewCommentPage(postID, first, after)})
	if err != nil {
		return nil, err
	}
	return pages[0], nil
}
func (s *PostgresStore) GetRepliesByParentID(ctx context.Context, parentID string, first int, after *string) (*model.CommentConnection, error) {
	pages, err := s.GetReplyPages(ctx, []CommentPage{NewCommentPage(parentID, first, after)})
	if err != nil {
		return nil, err
	}
	return pages[0], nil
}

func (s *PostgresStore) Subscribe(ctx context.Context, postID string) (<-chan *model.Comment, error) {
	ch := make(chan *model.Comment, 16)

	conn, err := pgx.Connect(ctx, s.connString)
	if err != nil {
		return nil, fmt.Errorf("failed to create listener connection: %w", err)
	}

	channelName := "post_" + postID
	if _, err := conn.Exec(ctx, "LISTEN "+pgx.Identifier{channelName}.Sanitize()); err != nil {
		conn.Close(ctx)
		return nil, fmt.Errorf("failed to LISTEN: %w", err)
	}

	go func() {
		defer conn.Close(context.Background())
		defer close(ch)

		for {
			notification, err := conn.WaitForNotification(ctx)
			if err != nil {
				return
			}

			var comment model.Comment
			if err := json.Unmarshal([]byte(notification.Payload), &comment); err != nil {
				continue
			}

			select {
			case ch <- &comment:
			case <-ctx.Done():
				return
			}
		}
	}()

	return ch, nil
}
