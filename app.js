
const startButton = document.getElementById("startBtn");
const status = document.getElementById("status");
const welcome = document.getElementById("welcome");
const quizForm = document.getElementById("quizForm");
const questionsContainer = document.getElementById("questions");

let questions = [];

startButton.addEventListener("click", async () => {
    const name = document.getElementById("studentName").value.trim();
    const studentId = document.getElementById("studentId").value.trim();

    if (!name || !studentId) {
        status.textContent = "Please enter your name and student ID.";
        return;
    }

    startButton.disabled = true;
    status.textContent = "Loading quiz questions...";

    try {
        const response = await fetch("/api/questions");

        if (!response.ok) {
            throw new Error("Could not load questions.");
        }

        questions = await response.json();
        questionsContainer.replaceChildren();

        questions.forEach((question, index) => {
            const section = document.createElement("section");
            section.className = "question";

            const heading = document.createElement("h3");
            heading.textContent = `${index + 1}. ${question.question}`;
            section.appendChild(heading);

            question.options.forEach((option, optionIndex) => {
                const label = document.createElement("label");
                label.className = "option";

                const radio = document.createElement("input");
                radio.type = "radio";
                radio.name = `question-${question.id}`;
                radio.value = String(optionIndex);
                radio.required = true;

                label.append(radio, document.createTextNode(" " + option));
                section.appendChild(label);
            });

            questionsContainer.appendChild(section);
        });

        welcome.hidden = true;
        quizForm.hidden = false;
        status.textContent = `Quiz loaded! ${questions.length} questions are ready.`;

    } catch (error) {
        status.textContent = error.message;
    } finally {
        startButton.disabled = false;
    }
});

quizForm.addEventListener("submit", (event) => {
    event.preventDefault();

    status.textContent =
        "Your answers are selected. The next step is connecting submission to the C++ grading API.";
});
