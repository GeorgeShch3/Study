package com.george.taskmanager.service;

import static org.springframework.http.HttpStatus.*;

import com.george.taskmanager.dto.Api.*;
import com.george.taskmanager.entity.*;
import com.george.taskmanager.exception.ApiException;
import com.george.taskmanager.repository.*;

import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;

@Service
@Transactional(readOnly = true)
public class ProjectService {
    private final ProjectRepository projects;
    private final MembershipRepository members;
    private final UserRepository users;
    private final AccountService accounts;

    public ProjectService(
            ProjectRepository projects,
            MembershipRepository members,
            UserRepository users,
            AccountService accounts) {
        this.projects = projects;
        this.members = members;
        this.users = users;
        this.accounts = accounts;
    }

    @Transactional
    public ProjectView create(String username, NewProject r) {
        var user = accounts.current(username);
        var project = projects.save(new Project(r.name().trim(), user));
        members.save(new Membership(project, user));
        return ProjectView.of(project);
    }

    public List<ProjectView> list(String username) {
        return members.findByUserIdOrderByIdAsc(accounts.current(username).getId()).stream()
                .map(m -> ProjectView.of(m.getProject()))
                .toList();
    }

    public Project accessible(Long id, AppUser user) {
        if (!members.existsByProjectIdAndUserId(id, user.getId()))
            throw new ApiException(NOT_FOUND, "Project not found");
        return projects.findById(id)
                .orElseThrow(() -> new ApiException(NOT_FOUND, "Project not found"));
    }

    public void owner(Project p, AppUser user) {
        if (!p.getOwner().getId().equals(user.getId()))
            throw new ApiException(FORBIDDEN, "Only the project owner can perform this action");
    }

    public AppUser member(Project p, Long userId) {
        if (userId == null) return null;
        if (!members.existsByProjectIdAndUserId(p.getId(), userId))
            throw new ApiException(BAD_REQUEST, "Assignee and reviewer must belong to the project");
        return users.findById(userId)
                .orElseThrow(() -> new ApiException(BAD_REQUEST, "User not found"));
    }

    @Transactional
    public MemberView add(Long id, String username, AddMember r) {
        var actor = accounts.current(username);
        var p = accessible(id, actor);
        owner(p, actor);
        var user =
                users.findByUsername(r.username())
                        .orElseThrow(() -> new ApiException(NOT_FOUND, "User not found"));
        if (members.existsByProjectIdAndUserId(id, user.getId()))
            throw new ApiException(CONFLICT, "User already belongs to project");
        members.saveAndFlush(new Membership(p, user));
        return new MemberView(user.getId(), user.getUsername(), "MEMBER");
    }

    public List<MemberView> members(Long id, String username) {
        var p = accessible(id, accounts.current(username));
        return members.findByProjectIdOrderByIdAsc(id).stream()
                .map(
                        m ->
                                new MemberView(
                                        m.getUser().getId(),
                                        m.getUser().getUsername(),
                                        p.getOwner().getId().equals(m.getUser().getId())
                                                ? "OWNER"
                                                : "MEMBER"))
                .toList();
    }
}
