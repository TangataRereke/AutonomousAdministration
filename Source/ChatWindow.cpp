#include "ChatWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollBar>
#include <QTextCursor>

ChatWindow::ChatWindow(QWidget *parent)
    : QWidget(parent),
      modelCombo_(new QComboBox(this)),
      chatView_(new QTextBrowser(this)),
      input_(new QLineEdit(this)),
      sendButton_(new QPushButton("Send", this)),
      stopButton_(new QPushButton("Stop", this)),
      network_(new QNetworkAccessManager(this))
{
    setWindowTitle("Ollama Chat (Local)");

    auto *mainLayout = new QVBoxLayout(this);

    // Model selection row
    auto *modelLayout = new QHBoxLayout();
    modelLayout->addWidget(new QLabel("Model:", this));
    modelLayout->addWidget(modelCombo_);
    mainLayout->addLayout(modelLayout);

    // Chat view
    chatView_->setReadOnly(true);
    chatView_->setOpenExternalLinks(true);
    mainLayout->addWidget(chatView_, 1);

    // Input row
    auto *inputLayout = new QHBoxLayout();
    inputLayout->addWidget(input_, 1);
    inputLayout->addWidget(sendButton_);
    inputLayout->addWidget(stopButton_);
    mainLayout->addLayout(inputLayout);

    // Initially disable the stop button
    stopButton_->setEnabled(false);

    // Connections
    connect(sendButton_, &QPushButton::clicked, this, &ChatWindow::sendMessage);
    connect(input_, &QLineEdit::returnPressed, this, &ChatWindow::sendMessage);
    connect(stopButton_, &QPushButton::clicked, this, &ChatWindow::stopGeneration);

    loadModels();
}

// ------------------------------------------------------------
// Model loading (unchanged, but with error display)
// ------------------------------------------------------------
void ChatWindow::loadModels()
{
    QUrl url("http://localhost:11434/api/tags");
    QNetworkRequest req(url);
    auto *reply = network_->get(req);
    connect(reply, &QNetworkReply::finished, this, &ChatWindow::handleModelsReply);
}

void ChatWindow::handleModelsReply()
{
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        chatView_->append("<b>Error loading models:</b> " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        chatView_->append("<b>Error:</b> Invalid JSON from /api/tags");
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray models = root.value("models").toArray();

    modelCombo_->clear();
    for (const auto &m : models) {
        QJsonObject obj = m.toObject();
        QString name = obj.value("name").toString();
        if (!name.isEmpty())
            modelCombo_->addItem(name);
    }

    if (modelCombo_->count() > 0) {
        currentModel_ = modelCombo_->currentText();
        chatView_->append("Loaded " + QString::number(modelCombo_->count()) + " models.");
    } else {
        chatView_->append("<b>Warning:</b> No models found. Is Ollama running?");
    }

    connect(modelCombo_, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        currentModel_ = text;
    });
}

// ------------------------------------------------------------
// Sending a message with streaming enabled
// ------------------------------------------------------------
void ChatWindow::sendMessage()
{
    QString text = input_->text().trimmed();
    if (text.isEmpty() || currentModel_.isEmpty())
        return;

    input_->clear();
    appendUserMessage(text);

    // Reset streaming state
    currentAssistantContent_.clear();
    buffer_.clear();

    // Build JSON payload
    QJsonObject root;
    root["model"] = currentModel_;

    QJsonArray messages;
    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = text;
    messages.append(userMsg);
    root["messages"] = messages;
    root["stream"] = true;   // streaming ON

    QJsonDocument doc(root);
    QByteArray payload = doc.toJson();

    QUrl url("http://localhost:11434/api/chat");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    currentReply_ = network_->post(req, payload);
    connect(currentReply_, &QNetworkReply::readyRead, this, &ChatWindow::handleChatChunk);
    connect(currentReply_, &QNetworkReply::finished, this, &ChatWindow::handleChatFinished);

    // UI state
    sendButton_->setEnabled(false);
    stopButton_->setEnabled(true);

    // Insert a placeholder for the assistant's reply
    chatView_->append("<b>Assistant:</b>");
    // Move cursor to the end and store it
    assistantCursor_ = QTextCursor(chatView_->document());
    assistantCursor_.movePosition(QTextCursor::End);
    assistantCursor_.insertHtml("<span id='assistant-content'></span>");
    chatView_->verticalScrollBar()->setValue(chatView_->verticalScrollBar()->maximum());
}

// ------------------------------------------------------------
// Streaming: process each chunk as it arrives
// ------------------------------------------------------------
void ChatWindow::handleChatChunk()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;

    buffer_.append(reply->readAll());

    while (true) {
        int newline = buffer_.indexOf('\n');
        if (newline == -1) break;
        QByteArray line = buffer_.left(newline).trimmed();
        buffer_.remove(0, newline + 1);
        if (line.isEmpty()) continue;

        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject()) continue;
        QJsonObject obj = doc.object();

        // Check for error
        if (obj.contains("error")) {
            appendAssistantMessage("[Error] " + obj.value("error").toString());
            reply->abort();
            return;
        }

        // Extract content chunk
        QJsonObject msg = obj.value("message").toObject();
        QString chunk = msg.value("content").toString();
        if (!chunk.isEmpty()) {
            currentAssistantContent_.append(chunk);
            appendToAssistantDisplay(chunk);  // Changed!
        }
    }
}

// ------------------------------------------------------------
// Update the displayed assistant content (replace placeholder)
// ------------------------------------------------------------
void ChatWindow::updateAssistantDisplay()
{
    // Move the stored cursor to the start of the placeholder block
    assistantCursor_.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    assistantCursor_.select(QTextCursor::BlockUnderCursor);
    // Replace with the current accumulated content (converted to HTML)
    assistantCursor_.insertHtml(markdownToHtml(currentAssistantContent_));
    // Scroll to bottom
    chatView_->verticalScrollBar()->setValue(chatView_->verticalScrollBar()->maximum());
}

// ------------------------------------------------------------
// Finished (either success, network error, or user stop)
// ------------------------------------------------------------
void ChatWindow::handleChatFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;

    sendButton_->setEnabled(true);
    stopButton_->setEnabled(false);

    if (reply->error() == QNetworkReply::NoError) {
        // Process any leftover data in buffer (last line might not have newline)
        if (!buffer_.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(buffer_);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QJsonObject msg = obj.value("message").toObject();
                QString chunk = msg.value("content").toString();
                if (!chunk.isEmpty()) {
                    currentAssistantContent_.append(chunk);
                    updateAssistantDisplay();
                }
            }
        }
        // Final update and separator
        updateAssistantDisplay();
        chatView_->append("<hr/>");
    } else if (reply->error() == QNetworkReply::OperationCanceledError) {
        // User clicked Stop
        if (!currentAssistantContent_.isEmpty()) {
            updateAssistantDisplay();
            chatView_->append("<i>... generation stopped by user</i>");
        } else {
            appendAssistantMessage("[Generation cancelled]");
        }
        chatView_->append("<hr/>");
    } else {
        // Network error
        appendAssistantMessage("[Network Error] " + reply->errorString());
        chatView_->append("<hr/>");
    }

    reply->deleteLater();
    currentReply_ = nullptr;
    buffer_.clear();
    currentAssistantContent_.clear();
}

// ------------------------------------------------------------
// Stop button: abort the current request
// ------------------------------------------------------------
void ChatWindow::stopGeneration()
{
    if (currentReply_) {
        currentReply_->abort();
    }
}

// ------------------------------------------------------------
// Helper functions to append user/assistant messages (non‑streaming)
// ------------------------------------------------------------
void ChatWindow::appendUserMessage(const QString &text)
{
    chatView_->append("<b>You:</b>");
    chatView_->append(markdownToHtml(text));
    chatView_->append("<hr/>");
    chatView_->verticalScrollBar()->setValue(chatView_->verticalScrollBar()->maximum());
}

void ChatWindow::appendToAssistantDisplay(const QString &text)
{
    // Find the last "assistant" block and append to it
    QTextDocument *doc = chatView_->document();
    QTextCursor cursor(doc);

    bool found = false;
    while (!found && cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor)) {
        if (cursor.block().text().startsWith("<b>Assistant:</b>")) {
            found = true;
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
            cursor.insertText(text);  // Append directly to the assistant block
        }
    }

    chatView_->verticalScrollBar()->setValue(chatView_->verticalScrollBar()->maximum());
}

void ChatWindow::appendAssistantMessage(const QString &text)
{
    chatView_->append("<b>Assistant:</b>");
    chatView_->append(markdownToHtml(text));
    chatView_->append("<hr/>");
    chatView_->verticalScrollBar()->setValue(chatView_->verticalScrollBar()->maximum());
}

// ------------------------------------------------------------
// Simple Markdown‑to‑HTML converter (unchanged)
// ------------------------------------------------------------
QString ChatWindow::markdownToHtml(const QString &text)
{
    QString html;
    QStringList lines = text.split('\n');
    bool inCodeBlock = false;

    for (const QString &line : lines) {
        if (line.trimmed().startsWith("```")) {
            inCodeBlock = !inCodeBlock;
            if (inCodeBlock) {
                html += "<pre style=\"background:#222;color:#eee;padding:6px;border-radius:4px;\"><code>";
            } else {
                html += "</code></pre>";
            }
            continue;
        }

        if (inCodeBlock) {
            QString escaped = line.toHtmlEscaped();
            html += escaped + "\n";
        } else {
            html += "<p>" + line.toHtmlEscaped() + "</p>";
        }
    }

    if (inCodeBlock) {
        html += "</code></pre>";
    }

    return html;
}