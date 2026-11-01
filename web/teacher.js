const logoutBtn = document.getElementById("logoutBtn");
const message = document.getElementById("message");
const resultsBody = document.getElementById("resultsBody");
const totalSubmissions = document.getElementById("totalSubmissions");
const averageScore = document.getElementById("averageScore");
const refreshBtn = document.getElementById("refreshBtn");
const exportBtn = document.getElementById("exportBtn");
const questionForm = document.getElementById("questionForm");
const questionId = document.getElementById("questionId");
const questionText = document.getElementById("questionText");
const correctAnswer = document.getElementById("correctAnswer");
const questionsList = document.getElementById("questionsList");
const questionMessage = document.getElementById("questionMessage");
const saveQuestionBtn = document.getElementById("saveQuestionBtn");
const cancelEditBtn = document.getElementById("cancelEditBtn");

let questions = [];

async function apiRequest(url, options = {}) {
    const response = await fetch(url, {
        cache: "no-store",
        credentials: "same-origin",
        ...options,
        headers: {
            "Content-Type": "application/json",
            ...(options.headers || {})
        }
    });

    if (response.status === 401) {
        window.location.replace("/teacher-login.html");
        throw new Error("Teacher login required.");
    }

    const data = await response.json().catch(() => ({}));

    if (!response.ok) {
        throw new Error(data.error || data.message || "Request failed.");
    }

    return data;
}


async function loadResults() {
    refreshBtn.disabled = true;
    message.textContent = "Loading student results...";

    try {
        const results = await apiRequest("/api/results");

        if (!Array.isArray(results)) {
            throw new Error("The server returned an invalid results list.");
        }

        resultsBody.replaceChildren();
        totalSubmissions.textContent = results.length;

        const totalMarks = results.reduce(
            (sum, result) => sum + result.total,
            0
        );

        const earnedMarks = results.reduce(
            (sum, result) => sum + result.score,
            0
        );

        averageScore.textContent =
            results.length > 0 && totalMarks > 0
                ? `${(earnedMarks / results.length).toFixed(2)}/${(totalMarks / results.length).toFixed(2)
                }`
                : "0/0";

        if (results.length === 0) {
            const row = document.createElement("tr");
            const cell = document.createElement("td");
            cell.colSpan = 7;
            cell.textContent = "No student submissions found yet.";
            row.appendChild(cell);
            resultsBody.appendChild(row);
            message.textContent = "No results available yet.";
            return;
        }

        // Calculate each attempt's percentage and retain its original order.
        const rankedResults = results.map((result, index) => ({
            ...result,
            originalIndex: index,
            percentage: Number(result.total) > 0
                ? (Number(result.score) / Number(result.total)) * 100
                : null
        }));

        // Rank by percentage, highest first.
        const sortedResults = [...rankedResults].sort((a, b) => {
            if (a.percentage === null) return b.percentage === null ? 0 : 1;
            if (b.percentage === null) return -1;
            return b.percentage - a.percentage;
        });

        // Competition ranking: 1, 1, 3 when two attempts tie for first.
        const rankByOriginalIndex = new Map();
        let previousPercentage = null;
        let currentRank = 0;

        sortedResults.forEach((result, index) => {
            let rank = "N/A";

            if (result.percentage !== null) {
                if (
                    index === 0 ||
                    previousPercentage === null ||
                    result.percentage !== previousPercentage
                ) {
                    currentRank = index + 1;
                }

                rank = currentRank;
                previousPercentage = result.percentage;
            }

            rankByOriginalIndex.set(result.originalIndex, rank);
        });

        // Display attempts in their original submission order.
        rankedResults.forEach((result, index) => {
            const row = document.createElement("tr");

            const percentage = result.percentage !== null
                ? `${result.percentage.toFixed(1)}%`
                : "N/A";

            [
                index + 1,
                rankByOriginalIndex.get(index),
                result.name,
                result.studentId,
                `${result.score}/${result.total}`,
                percentage,
                result.timestamp || "Unavailable"
            ].forEach(value => {
                const cell = document.createElement("td");
                cell.textContent = String(value);
                row.appendChild(cell);
            });

            resultsBody.appendChild(row);
        });

        message.textContent = "Results loaded successfully.";
    } catch (error) {
        message.textContent = error.message;
        resultsBody.replaceChildren();

        const row = document.createElement("tr");
        const cell = document.createElement("td");
        cell.colSpan = 7;
        cell.textContent = "Unable to display results. Try refreshing.";
        row.appendChild(cell);
        resultsBody.appendChild(row);
    } finally {
        refreshBtn.disabled = false;
    }
}


function resetQuestionForm() {
    questionForm.reset();
    questionId.value = "";
    saveQuestionBtn.textContent = "Add Question";
    cancelEditBtn.hidden = true;
}

async function loadQuestions() {
    questionsList.textContent = "Loading questions...";

    try {
        const data = await apiRequest("/api/teacher/questions");

        if (!Array.isArray(data)) {
            throw new Error("Invalid question list returned by the server.");
        }

        questions = data;
        renderQuestions();
    } catch (error) {
        questionsList.textContent = error.message;
        questionMessage.textContent = "Could not load questions.";
    }
}

function renderQuestions() {
    questionsList.replaceChildren();

    if (questions.length === 0) {
        questionsList.textContent = "No questions available.";
        return;
    }

    questions.forEach(question => {
        const card = document.createElement("article");
        card.className = "question-card";

        const heading = document.createElement("h4");
        heading.textContent = `Question ${question.id}: ${question.question}`;
        card.appendChild(heading);

        const options = document.createElement("ol");
        options.className = "question-options";

        question.options.forEach((option, index) => {
            const item = document.createElement("li");
            item.textContent = `${option}${index === question.correctAnswer ? " (Correct answer)" : ""
                }`;
            options.appendChild(item);
        });

        card.appendChild(options);

        const editButton = document.createElement("button");
        editButton.type = "button";
        editButton.textContent = "Edit";
        editButton.addEventListener("click", () => {
            questionId.value = question.id;
            questionText.value = question.question;

            question.options.forEach((option, index) => {
                document.getElementById(`option${index}`).value = option;
            });

            correctAnswer.value = String(question.correctAnswer);
            saveQuestionBtn.textContent = "Save Changes";
            cancelEditBtn.hidden = false;
            questionMessage.textContent = `Editing question ${question.id}.`;

            questionForm.scrollIntoView({
                behavior: "smooth",
                block: "start"
            });
            questionText.focus();
        });

        card.appendChild(editButton);

        const deleteButton = document.createElement("button");
        deleteButton.type = "button";
        deleteButton.textContent = "Delete";
        deleteButton.addEventListener("click", async () => {
            if (!confirm(`Delete question ${question.id}?`)) {
                return;
            }

            deleteButton.disabled = true;

            try {
                await apiRequest("/api/teacher/questions", {
                    method: "DELETE",
                    body: JSON.stringify({ id: question.id })
                });

                questionMessage.textContent = "Question deleted successfully.";
                resetQuestionForm();
                await loadQuestions();
            } catch (error) {
                questionMessage.textContent = error.message;
                deleteButton.disabled = false;
            }
        });

        card.appendChild(deleteButton);
        questionsList.appendChild(card);
    });
}

questionForm.addEventListener("submit", async event => {
    event.preventDefault();

    const payload = {
        question: questionText.value.trim(),
        options: [0, 1, 2, 3].map(index =>
            document.getElementById(`option${index}`).value.trim()
        ),
        correctAnswer: Number(correctAnswer.value)
    };

    if (
        !payload.question ||
        payload.options.some(option => !option)
    ) {
        questionMessage.textContent =
            "Enter a question and fill in all four options.";
        return;
    }

    const editing = questionId.value !== "";

    if (editing) {
        payload.id = Number(questionId.value);
    }

    saveQuestionBtn.disabled = true;

    try {
        await apiRequest("/api/teacher/questions", {
            method: editing ? "PUT" : "POST",
            body: JSON.stringify(payload)
        });

        questionMessage.textContent = editing
            ? "Question updated successfully."
            : "Question added successfully.";

        resetQuestionForm();
        await loadQuestions();
    } catch (error) {
        questionMessage.textContent = error.message;
    } finally {
        saveQuestionBtn.disabled = false;
    }
});

cancelEditBtn.addEventListener("click", () => {
    resetQuestionForm();
    questionMessage.textContent = "Editing cancelled.";
});


function csvEscape(value) {
    const text = String(value ?? "");
    return `"${text.replace(/"/g, '""')}"`;
}

exportBtn.addEventListener("click", async () => {
    exportBtn.disabled = true;
    message.textContent = "Preparing CSV export...";

    try {
        const results = await apiRequest("/api/results");

        if (!Array.isArray(results)) {
            throw new Error("The server returned an invalid results list.");
        }

        const rankedResults = results.map((result, index) => ({
            ...result,
            originalIndex: index,
            percentage: Number(result.total) > 0
                ? Number(result.score) / Number(result.total) * 100
                : null
        }));

        const sortedResults = [...rankedResults].sort((a, b) => {
            if (a.percentage === null) {
                return b.percentage === null ? 0 : 1;
            }
            if (b.percentage === null) return -1;
            return b.percentage - a.percentage;
        });

        const ranks = new Map();
        let currentRank = 0;
        let previousPercentage = null;

        sortedResults.forEach((result, index) => {
            let rank = "N/A";

            if (result.percentage !== null) {
                if (
                    index === 0 ||
                    previousPercentage === null ||
                    result.percentage !== previousPercentage
                ) {
                    currentRank = index + 1;
                }

                rank = currentRank;
                previousPercentage = result.percentage;
            }

            ranks.set(result.originalIndex, rank);
        });

        const rows = [
            [
                "Attempt",
                "Rank",
                "Student Name",
                "Student ID",
                "Score",
                "Total",
                "Percentage",
                "Submitted At"
            ],
            ...rankedResults.map((result, index) => [
                index + 1,
                ranks.get(index),
                result.name,
                result.studentId,
                result.score,
                result.total,
                result.percentage === null
                    ? "N/A"
                    : `${result.percentage.toFixed(1)}%`,
                result.timestamp || "Unavailable"
            ])
        ];

        const csv = "\uFEFF" + rows
            .map(row => row.map(csvEscape).join(","))
            .join("\r\n");

        const blob = new Blob([csv], {
            type: "text/csv;charset=utf-8;"
        });

        const url = URL.createObjectURL(blob);
        const link = document.createElement("a");
        link.href = url;
        link.download = "quiz-results.csv";
        document.body.appendChild(link);
        link.click();
        link.remove();
        URL.revokeObjectURL(url);

        message.textContent = "CSV exported successfully.";
    } catch (error) {
        message.textContent = error.message;
    } finally {
        exportBtn.disabled = false;
    }
});


refreshBtn.addEventListener("click", loadResults);

logoutBtn.addEventListener("click", async () => {
    logoutBtn.disabled = true;

    try {
        await fetch("/api/teacher/logout", {
            method: "POST",
            credentials: "same-origin"
        });
    } finally {
        window.location.replace("/teacher-login.html");
    }
});

loadResults();
loadQuestions();
