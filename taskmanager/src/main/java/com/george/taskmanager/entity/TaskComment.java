package com.george.taskmanager.entity;

import jakarta.persistence.*;

@Entity
@Table(name = "task_comments")
public class TaskComment {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "task_id", nullable = false)
    private Task task;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "author_id", nullable = false)
    private AppUser author;

    @Column(nullable = false, length = 2000)
    private String body;

    @Column(nullable = false)
    private java.time.Instant createdAt;

    protected TaskComment() {}

    public TaskComment(Task task, AppUser author, String body) {
        this.task = task;
        this.author = author;
        this.body = body;
        this.createdAt = java.time.Instant.now();
    }

    public Long getId() {
        return id;
    }

    public Task getTask() {
        return task;
    }

    public AppUser getAuthor() {
        return author;
    }

    public String getBody() {
        return body;
    }

    public java.time.Instant getCreatedAt() {
        return createdAt;
    }
}
