package com.george.taskmanager.repository;

import com.george.taskmanager.entity.*;

import org.springframework.data.jpa.repository.JpaRepository;

public interface EventRepository extends JpaRepository<TaskEvent, Long> {
    org.springframework.data.domain.Page<TaskEvent> findByTaskId(
            Long taskId, org.springframework.data.domain.Pageable pageable);
}
