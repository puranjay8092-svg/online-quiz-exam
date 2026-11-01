
const loginForm = document.getElementById("loginForm");
const loginButton = document.getElementById("loginButton");
const loginMessage = document.getElementById("loginMessage");
const usernameInput = document.getElementById("username");
const passwordInput = document.getElementById("password");

loginForm.addEventListener("submit", async (event) => {
    event.preventDefault();

    loginButton.disabled = true;
    loginMessage.textContent = "Checking credentials...";

    try {
        const response = await fetch("/api/teacher/login", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            credentials: "same-origin",
            body: JSON.stringify({
                username: usernameInput.value,
                password: passwordInput.value
            })
        });

        const data = await response.json();

        if (!response.ok) {
            throw new Error(data.error || "Login failed.");
        }

        passwordInput.value = "";
        window.location.replace("/teacher.html");

    } catch (error) {
        loginMessage.textContent =
            error.message || "Unable to log in. Try again.";
    } finally {
        loginButton.disabled = false;
    }
});
