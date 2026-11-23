package com.george.taskmanager;

import static org.assertj.core.api.Assertions.assertThat;
import static org.springframework.security.test.web.servlet.request.SecurityMockMvcRequestPostProcessors.httpBasic;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.*;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.*;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.webmvc.test.autoconfigure.AutoConfigureMockMvc;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.ResultActions;
import org.springframework.test.web.servlet.request.MockHttpServletRequestBuilder;

import tools.jackson.databind.JsonNode;
import tools.jackson.databind.ObjectMapper;

import java.util.Map;
import java.util.UUID;

@SpringBootTest
@AutoConfigureMockMvc
class WorkflowIntegrationTests {
    private static final String PASSWORD = "test-password-123";
    @Autowired MockMvc mvc;
    @Autowired ObjectMapper json;
    @Autowired com.george.taskmanager.repository.UserRepository users;
    @Autowired org.springframework.security.crypto.password.PasswordEncoder encoder;
    String owner, worker, reviewer, outsider;
    long workerId, reviewerId, outsiderId, projectId, taskId;

    @BeforeEach
    void setup() throws Exception {
        String suffix = UUID.randomUUID().toString().replace("-", "").substring(0, 12);
        owner = "owner_" + suffix;
        worker = "worker_" + suffix;
        reviewer = "reviewer_" + suffix;
        outsider = "outsider_" + suffix;
        register(owner);
        workerId = register(worker);
        reviewerId = register(reviewer);
        outsiderId = register(outsider);
        projectId =
                body(send(post("/api/projects"), owner, Map.of("name", "Release"))
                                .andExpect(status().isCreated()))
                        .path("id")
                        .asLong();
        for (String user : new String[] {worker, reviewer}) {
            send(post("/api/projects/" + projectId + "/members"), owner, Map.of("username", user))
                    .andExpect(status().isCreated());
        }
        taskId =
                body(send(
                                        post("/api/projects/" + projectId + "/tasks"),
                                        owner,
                                        Map.of(
                                                "title",
                                                "Ship release",
                                                "assigneeId",
                                                workerId,
                                                "reviewerId",
                                                reviewerId))
                                .andExpect(status().isCreated()))
                        .path("id")
                        .asLong();
    }

    @Test
    void authenticationAndCredentialStorage() throws Exception {
        String hash = users.findByUsername(owner).orElseThrow().getPasswordHash();
        assertThat(hash).isNotEqualTo(PASSWORD);
        assertThat(encoder.matches(PASSWORD, hash)).isTrue();
        assertThat(new com.george.taskmanager.dto.Api.Register(owner, PASSWORD).toString())
                .doesNotContain(PASSWORD);
        mvc.perform(get("/api/projects")).andExpect(status().isUnauthorized());
        mvc.perform(get("/api/auth/me").with(httpBasic(owner, "wrong-password")))
                .andExpect(status().isUnauthorized());
        send(get("/api/auth/me"), owner, null)
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.username").value(owner))
                .andExpect(jsonPath("$.passwordHash").doesNotExist());
        mvc.perform(
                        post("/api/auth/register")
                                .contentType(MediaType.APPLICATION_JSON)
                                .content(
                                        json.writeValueAsString(
                                                Map.of("username", owner, "password", PASSWORD))))
                .andExpect(status().isConflict());
    }

    @Test
    void crossProjectAccessIsDeniedEverywhere() throws Exception {
        for (String path :
                new String[] {
                    "/api/tasks/" + taskId,
                    "/api/tasks/" + taskId + "/comments",
                    "/api/tasks/" + taskId + "/history",
                    "/api/projects/" + projectId + "/tasks",
                    "/api/projects/" + projectId + "/members"
                }) {
            send(get(path), outsider, null).andExpect(status().isNotFound());
        }
        send(get("/api/projects"), outsider, null).andExpect(jsonPath("$.length()").value(0));
        send(patch("/api/tasks/" + taskId), outsider, Map.of("version", 0, "title", "Intrusion"))
                .andExpect(status().isNotFound());
        send(post("/api/tasks/" + taskId + "/comments"), outsider, Map.of("body", "Intrusion"))
                .andExpect(status().isNotFound());
        transition(outsider, 0, "IN_PROGRESS", null).andExpect(status().isNotFound());
        send(
                        put("/api/tasks/" + taskId + "/assignment"),
                        outsider,
                        Map.of("version", 0, "assigneeId", outsiderId))
                .andExpect(status().isNotFound());
        send(post("/api/projects/" + projectId + "/tasks"), outsider, Map.of("title", "Intrusion"))
                .andExpect(status().isNotFound());
    }

    @Test
    void onlyOwnerManagesMembershipAndAssignment() throws Exception {
        send(post("/api/projects/" + projectId + "/members"), worker, Map.of("username", outsider))
                .andExpect(status().isForbidden());
        send(
                        put("/api/tasks/" + taskId + "/assignment"),
                        worker,
                        Map.of("version", 0, "assigneeId", reviewerId, "reviewerId", workerId))
                .andExpect(status().isForbidden());
        send(
                        post("/api/projects/" + projectId + "/tasks"),
                        worker,
                        Map.of("title", "Self-assign", "assigneeId", workerId))
                .andExpect(status().isForbidden());
        send(
                        post("/api/projects/" + projectId + "/tasks"),
                        worker,
                        Map.of("title", "Unassigned suggestion"))
                .andExpect(status().isCreated());
    }

    @Test
    void invalidAssignmentsAreRejected() throws Exception {
        send(
                        put("/api/tasks/" + taskId + "/assignment"),
                        owner,
                        Map.of("version", 0, "assigneeId", outsiderId))
                .andExpect(status().isBadRequest());
        send(
                        put("/api/tasks/" + taskId + "/assignment"),
                        owner,
                        Map.of("version", 0, "assigneeId", workerId, "reviewerId", workerId))
                .andExpect(status().isBadRequest());
        send(get("/api/tasks/" + taskId), owner, null).andExpect(jsonPath("$.version").value(0));
    }

    @Test
    void fullReviewCycleAndAtomicReturnComment() throws Exception {
        transition(reviewer, 0, "IN_PROGRESS", null).andExpect(status().isForbidden());
        transition(worker, 0, "IN_PROGRESS", null)
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.version").value(1));
        transition(worker, 1, "DONE", null).andExpect(status().isConflict());
        transition(worker, 1, "REVIEW", "Ready for review").andExpect(status().isOk());
        transition(worker, 2, "DONE", null).andExpect(status().isForbidden());
        transition(owner, 2, "DONE", null).andExpect(status().isForbidden());
        transition(reviewer, 2, "IN_PROGRESS", " ").andExpect(status().isBadRequest());
        send(get("/api/tasks/" + taskId), owner, null)
                .andExpect(jsonPath("$.status").value("REVIEW"))
                .andExpect(jsonPath("$.version").value(2));
        send(get("/api/tasks/" + taskId + "/comments"), owner, null)
                .andExpect(jsonPath("$.totalElements").value(1));
        transition(reviewer, 2, "IN_PROGRESS", "Please add tests").andExpect(status().isOk());
        transition(worker, 3, "REVIEW", null).andExpect(status().isOk());
        transition(reviewer, 4, "DONE", "Accepted")
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.version").value(5));
        send(get("/api/tasks/" + taskId + "/history"), worker, null)
                .andExpect(jsonPath("$.totalElements").value(6))
                .andExpect(jsonPath("$.items[0].details").value("REVIEW -> DONE"));
        send(get("/api/tasks/" + taskId + "/comments"), worker, null)
                .andExpect(jsonPath("$.totalElements").value(3));
    }

    @Test
    void staleVersionCannotOverwriteContentOrAppendEvents() throws Exception {
        send(patch("/api/tasks/" + taskId), owner, Map.of("version", 0, "title", "New title"))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.version").value(1));
        send(patch("/api/tasks/" + taskId), owner, Map.of("version", 0, "title", "Lost update"))
                .andExpect(status().isConflict());
        transition(worker, 0, "IN_PROGRESS", "Stale comment").andExpect(status().isConflict());
        send(get("/api/tasks/" + taskId), worker, null)
                .andExpect(jsonPath("$.title").value("New title"));
        send(get("/api/tasks/" + taskId + "/history"), worker, null)
                .andExpect(jsonPath("$.totalElements").value(2));
        send(get("/api/tasks/" + taskId + "/comments"), worker, null)
                .andExpect(jsonPath("$.totalElements").value(0));
    }

    @Test
    void reviewContentIsFrozenAndAssigneeCannotEditOwnerContent() throws Exception {
        send(patch("/api/tasks/" + taskId), worker, Map.of("version", 0, "title", "Change"))
                .andExpect(status().isForbidden());
        transition(worker, 0, "IN_PROGRESS", null).andExpect(status().isOk());
        transition(worker, 1, "REVIEW", null).andExpect(status().isOk());
        send(
                        patch("/api/tasks/" + taskId),
                        owner,
                        Map.of("version", 2, "title", "Changed during review"))
                .andExpect(status().isConflict());
        send(put("/api/tasks/" + taskId + "/assignment"), owner, Map.of("version", 2))
                .andExpect(status().isConflict());
    }

    @Test
    void reviewerMustBeAssignedBeforeSubmission() throws Exception {
        send(
                        put("/api/tasks/" + taskId + "/assignment"),
                        owner,
                        Map.of("version", 0, "assigneeId", workerId))
                .andExpect(status().isOk());
        transition(worker, 1, "IN_PROGRESS", null).andExpect(status().isOk());
        transition(worker, 2, "REVIEW", null).andExpect(status().isConflict());
        send(put("/api/tasks/" + taskId + "/assignment"), owner, Map.of("version", 2))
                .andExpect(status().isConflict());
    }

    @Test
    void filteringPaginationAndValidation() throws Exception {
        transition(worker, 0, "IN_PROGRESS", null).andExpect(status().isOk());
        send(get("/api/projects/" + projectId + "/tasks?status=TODO"), owner, null)
                .andExpect(jsonPath("$.totalElements").value(0));
        send(get("/api/projects/" + projectId + "/tasks?status=IN_PROGRESS&size=1"), owner, null)
                .andExpect(jsonPath("$.items[0].id").value(taskId))
                .andExpect(jsonPath("$.totalPages").value(1));
        send(get("/api/projects/" + projectId + "/tasks?size=101"), owner, null)
                .andExpect(status().isBadRequest());
        send(post("/api/tasks/" + taskId + "/comments"), worker, Map.of("body", "  "))
                .andExpect(status().isBadRequest());
        send(patch("/api/tasks/" + taskId), owner, Map.of("title", "Missing version"))
                .andExpect(status().isBadRequest());
        send(
                        post("/api/tasks/" + taskId + "/transitions"),
                        worker,
                        Map.of("version", 1, "status", "UNKNOWN"))
                .andExpect(status().isBadRequest());
    }

    @Test
    void auditStoresMaximumLengthDescriptionsWithoutTruncation() throws Exception {
        String original = "a".repeat(2000), updated = "b".repeat(2000);
        send(patch("/api/tasks/" + taskId), owner, Map.of("version", 0, "description", original))
                .andExpect(status().isOk());
        send(patch("/api/tasks/" + taskId), owner, Map.of("version", 1, "description", updated))
                .andExpect(status().isOk());
        JsonNode history =
                body(
                        send(get("/api/tasks/" + taskId + "/history"), owner, null)
                                .andExpect(status().isOk()));
        assertThat(history.path("items").get(0).path("details").asText())
                .contains(original, updated);
    }

    @Test
    void concurrentEditsHaveExactlyOneWinner() throws Exception {
        var start = new java.util.concurrent.CountDownLatch(1);
        var pool = java.util.concurrent.Executors.newFixedThreadPool(2);
        try {
            var results = new java.util.ArrayList<java.util.concurrent.Future<Integer>>();
            for (String title : new String[] {"First edit", "Second edit"}) {
                results.add(
                        pool.submit(
                                () -> {
                                    start.await();
                                    return send(
                                                    patch("/api/tasks/" + taskId),
                                                    owner,
                                                    Map.of("version", 0, "title", title))
                                            .andReturn()
                                            .getResponse()
                                            .getStatus();
                                }));
            }
            start.countDown();
            var statuses = new java.util.ArrayList<Integer>();
            for (var result : results)
                statuses.add(result.get(20, java.util.concurrent.TimeUnit.SECONDS));
            assertThat(statuses).containsExactlyInAnyOrder(200, 409);
            send(get("/api/tasks/" + taskId + "/history"), owner, null)
                    .andExpect(jsonPath("$.totalElements").value(2));
        } finally {
            pool.shutdownNow();
        }
    }

    @Test
    void passwordLimitsUseUtf8BytesAndSupportUnicode() throws Exception {
        String username =
                "unicode_" + UUID.randomUUID().toString().replace("-", "").substring(0, 12);
        mvc.perform(
                        post("/api/auth/register")
                                .contentType(MediaType.APPLICATION_JSON)
                                .content(
                                        json.writeValueAsString(
                                                Map.of(
                                                        "username",
                                                        username,
                                                        "password",
                                                        "я".repeat(37)))))
                .andExpect(status().isBadRequest());
        String password = "пароль_для_теста";
        mvc.perform(
                        post("/api/auth/register")
                                .contentType(MediaType.APPLICATION_JSON)
                                .content(
                                        json.writeValueAsString(
                                                Map.of(
                                                        "username",
                                                        username,
                                                        "password",
                                                        password))))
                .andExpect(status().isCreated());
        mvc.perform(get("/api/auth/me").with(httpBasic(username, password)))
                .andExpect(status().isOk());
    }

    private long register(String username) throws Exception {
        return body(mvc.perform(
                                post("/api/auth/register")
                                        .contentType(MediaType.APPLICATION_JSON)
                                        .content(
                                                json.writeValueAsString(
                                                        Map.of(
                                                                "username",
                                                                username,
                                                                "password",
                                                                PASSWORD))))
                        .andExpect(status().isCreated()))
                .path("id")
                .asLong();
    }

    private ResultActions send(MockHttpServletRequestBuilder request, String user, Object payload)
            throws Exception {
        request.with(httpBasic(user, PASSWORD));
        if (payload != null)
            request.contentType(MediaType.APPLICATION_JSON)
                    .content(json.writeValueAsString(payload));
        return mvc.perform(request);
    }

    private ResultActions transition(String user, long version, String status, String comment)
            throws Exception {
        var payload = new java.util.HashMap<String, Object>();
        payload.put("version", version);
        payload.put("status", status);
        if (comment != null) payload.put("comment", comment);
        return send(post("/api/tasks/" + taskId + "/transitions"), user, payload);
    }

    private JsonNode body(ResultActions result) throws Exception {
        return json.readTree(result.andReturn().getResponse().getContentAsString());
    }
}
