package com.george.taskmanager.entity;

import jakarta.persistence.*;

@Entity
@Table(
        name = "memberships",
        uniqueConstraints = @UniqueConstraint(columnNames = {"project_id", "user_id"}))
public class Membership {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "project_id", nullable = false)
    private Project project;

    @ManyToOne(fetch = FetchType.LAZY, optional = false)
    @JoinColumn(name = "user_id", nullable = false)
    private AppUser user;

    protected Membership() {}

    public Membership(Project project, AppUser user) {
        this.project = project;
        this.user = user;
    }

    public Long getId() {
        return id;
    }

    public Project getProject() {
        return project;
    }

    public AppUser getUser() {
        return user;
    }
}
