package com.george.taskmanager.repository;

import com.george.taskmanager.entity.*;

import org.springframework.data.jpa.repository.JpaRepository;

public interface MembershipRepository extends JpaRepository<Membership, Long> {
    boolean existsByProjectIdAndUserId(Long projectId, Long userId);

    java.util.List<Membership> findByUserIdOrderByIdAsc(Long userId);

    java.util.List<Membership> findByProjectIdOrderByIdAsc(Long projectId);
}
