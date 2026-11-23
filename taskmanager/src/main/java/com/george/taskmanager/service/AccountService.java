package com.george.taskmanager.service;

import com.george.taskmanager.dto.Api.*;
import com.george.taskmanager.entity.AppUser;
import com.george.taskmanager.exception.ApiException;
import com.george.taskmanager.repository.UserRepository;

import org.springframework.http.HttpStatus;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
@Transactional(readOnly = true)
public class AccountService {
    private final UserRepository users;
    private final PasswordEncoder encoder;

    public AccountService(UserRepository users, PasswordEncoder encoder) {
        this.users = users;
        this.encoder = encoder;
    }

    @Transactional
    public UserView register(Register r) {
        if (r.password().getBytes(java.nio.charset.StandardCharsets.UTF_8).length > 72)
            throw new ApiException(
                    HttpStatus.BAD_REQUEST, "Password must be at most 72 UTF-8 bytes");
        if (users.findByUsername(r.username()).isPresent())
            throw new ApiException(HttpStatus.CONFLICT, "Username already registered");
        return UserView.of(
                users.saveAndFlush(new AppUser(r.username(), encoder.encode(r.password()))));
    }

    public AppUser current(String username) {
        return users.findByUsername(username)
                .orElseThrow(
                        () -> new ApiException(HttpStatus.UNAUTHORIZED, "Authentication required"));
    }

    public UserView me(String username) {
        return UserView.of(current(username));
    }
}
