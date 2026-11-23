package com.george.taskmanager.entity;

import jakarta.persistence.*;

@Entity
@Table(name = "task_events")
public class TaskEvent {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "task_id", nullable = false)
    private Task task;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "actor_id", nullable = false)
    private AppUser actor;

    @Column(nullable = false, length = 40)
    private String type;

    @Column(nullable = false, length = 5000)
    private String details;

    @Column(nullable = false)
    private java.time.Instant createdAt;

    protected TaskEvent() {}

    public TaskEvent(Task task, AppUser actor, String type, String details) {
        this.task = task;
        this.actor = actor;
        this.type = type;
        this.details = details;
        this.createdAt = java.time.Instant.now();
    }

    public Long getId() {
        return id;
    }

    public Task getTask() {
        return task;
    }

    public AppUser getActor() {
        return actor;
    }

    public String getType() {
        return type;
    }

    public String getDetails() {
        return details;
    }

    public java.time.Instant getCreatedAt() {
        return createdAt;
    }
}
