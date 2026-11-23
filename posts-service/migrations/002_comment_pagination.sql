CREATE INDEX IF NOT EXISTS idx_comments_roots_page
ON comments(post_id, created_at DESC, id DESC) WHERE parent_id IS NULL;
CREATE INDEX IF NOT EXISTS idx_comments_replies_page
ON comments(parent_id, created_at, id) WHERE parent_id IS NOT NULL;
