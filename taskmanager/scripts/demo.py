#!/usr/bin/env python3
import base64
import json
import os
import uuid
import urllib.error
import urllib.request

BASE_URL = os.environ.get("BASE_URL", "http://localhost:8080").rstrip("/")
PASSWORD = "demo-password-123"


def request(method, path, payload=None, username=None):
    headers = {"Content-Type": "application/json"}
    if username:
        token = base64.b64encode(f"{username}:{PASSWORD}".encode()).decode()
        headers["Authorization"] = f"Basic {token}"
    data = json.dumps(payload).encode() if payload is not None else None
    req = urllib.request.Request(BASE_URL + path, data=data, headers=headers, method=method)
    with urllib.request.urlopen(req, timeout=15) as response:
        return json.load(response)


def main():
    suffix = uuid.uuid4().hex[:8]
    users = {}
    for role in ("owner", "worker", "reviewer"):
        users[role] = request("POST", "/api/auth/register", {
            "username": f"demo_{role}_{suffix}", "password": PASSWORD})
    owner, worker, reviewer = (users[r]["username"] for r in ("owner", "worker", "reviewer"))
    project = request("POST", "/api/projects", {"name": "Portfolio demo"}, owner)
    prefix = f'/api/projects/{project["id"]}'
    for username in (worker, reviewer):
        request("POST", prefix + "/members", {"username": username}, owner)
    task = request("POST", prefix + "/tasks", {
        "title": "Ship team task manager", "description": "Implement and review the release",
        "assigneeId": users["worker"]["id"], "reviewerId": users["reviewer"]["id"]}, owner)
    path = f'/api/tasks/{task["id"]}'
    for username, status, comment in (
        (worker, "IN_PROGRESS", None),
        (worker, "REVIEW", "Ready for review"),
        (reviewer, "IN_PROGRESS", "Please cover access control with tests"),
        (worker, "REVIEW", "Tests added"),
        (reviewer, "DONE", "Accepted"),
    ):
        task = request("POST", path + "/transitions", {
            "version": task["version"], "status": status, "comment": comment}, username)
        print(f'{username}: {status} (version {task["version"]})')
    history = request("GET", path + "/history", username=owner)
    print(f'\nProject {project["id"]}, task {task["id"]}: {task["status"]}')
    print(f'History: {history["totalElements"]} events')
    print(f'Swagger: {BASE_URL}/swagger-ui.html')
    print(f'Demo owner: {owner}; demo password: {PASSWORD}')


if __name__ == "__main__":
    try:
        main()
    except (urllib.error.URLError, TimeoutError) as exc:
        raise SystemExit(f"Demo failed: {exc}. Start the application and check BASE_URL.") from exc
