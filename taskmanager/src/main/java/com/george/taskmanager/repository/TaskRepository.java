package com.george.taskmanager.repository;

import com.george.taskmanager.entity.*;

import org.springframework.data.jpa.repository.JpaRepository;

public interface TaskRepository extends JpaRepository<Task, Long> {
    org.springframework.data.domain.Page<Task> findByProjectId(
            Long projectId, org.springframework.data.domain.Pageable pageable);

    org.springframework.data.domain.Page<Task> findByProjectIdAndStatus(
            Long projectId, TaskStatus status, org.springframework.data.domain.Pageable pageable);
}
