package com.george.taskmanager.repository;

import com.george.taskmanager.entity.*;

import org.springframework.data.jpa.repository.JpaRepository;

public interface CommentRepository extends JpaRepository<TaskComment, Long> {
    org.springframework.data.domain.Page<TaskComment> findByTaskId(
            Long taskId, org.springframework.data.domain.Pageable pageable);
}
