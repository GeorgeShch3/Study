package com.george.taskmanager.entity;

import jakarta.persistence.*;

@Entity
@Table(name = "tasks")
public class Task {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "project_id", nullable = false)
    private Project project;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "author_id", nullable = false)
    private AppUser author;

    @Column(nullable = false, length = 255)
    private String title;

    @Column(length = 2000)
    private String description;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "assignee_id")
    private AppUser assignee;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "reviewer_id")
    private AppUser reviewer;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false, length = 20)
    private TaskStatus status;

    @Version
    @Column(nullable = false)
    private Long version;

    @Column(nullable = false)
    private java.time.Instant createdAt;

    protected Task() {}

    public Task(
            Project project,
            AppUser author,
            String title,
            String description,
            AppUser assignee,
            AppUser reviewer) {
        this.project = project;
        this.author = author;
        this.title = title;
        this.description = description;
        this.assignee = assignee;
        this.reviewer = reviewer;
        this.status = TaskStatus.TODO;
        this.createdAt = java.time.Instant.now();
    }

    public Long getId() {
        return id;
    }

    public Project getProject() {
        return project;
    }

    public AppUser getAuthor() {
        return author;
    }

    public String getTitle() {
        return title;
    }

    public void setTitle(String title) {
        this.title = title;
    }

    public String getDescription() {
        return description;
    }

    public void setDescription(String description) {
        this.description = description;
    }

    public AppUser getAssignee() {
        return assignee;
    }

    public void setAssignee(AppUser assignee) {
        this.assignee = assignee;
    }

    public AppUser getReviewer() {
        return reviewer;
    }

    public void setReviewer(AppUser reviewer) {
        this.reviewer = reviewer;
    }

    public TaskStatus getStatus() {
        return status;
    }

    public void setStatus(TaskStatus status) {
        this.status = status;
    }

    public Long getVersion() {
        return version;
    }

    public java.time.Instant getCreatedAt() {
        return createdAt;
    }
}
