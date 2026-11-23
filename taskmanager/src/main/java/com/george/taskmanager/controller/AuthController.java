package com.george.taskmanager.controller;

import com.george.taskmanager.dto.Api.*;
import com.george.taskmanager.service.AccountService;

import jakarta.validation.Valid;

import org.springframework.http.HttpStatus;
import org.springframework.web.bind.annotation.*;

import java.security.Principal;

@RestController
@RequestMapping("/api/auth")
public class AuthController {
    private final AccountService accounts;

    public AuthController(AccountService accounts) {
        this.accounts = accounts;
    }

    @PostMapping("/register")
    @ResponseStatus(HttpStatus.CREATED)
    public UserView register(@Valid @RequestBody Register r) {
        return accounts.register(r);
    }

    @GetMapping("/me")
    public UserView me(Principal p) {
        return accounts.me(p.getName());
    }
}
