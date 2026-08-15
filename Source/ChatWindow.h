#pragma once

#include <QWidget>
#include <QNetworkAccessManager>
#include <QTextCursor>
#include <QTextBlock>      // ← add this
#include <QTextDocument>   // ← add this (optional but safe)

class QComboBox;
class QTextBrowser;
class QLineEdit;
class QPushButton;
class QNetworkReply;

class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWindow(QWidget *parent = nullptr);

private slots:
    void loadModels();
    void handleModelsReply();
    void sendMessage();
    void stopGeneration();

    // Streaming slots
    void handleChatChunk();
    void handleChatFinished();

private:
    void appendToAssistantDisplay(const QString &text);
    void appendUserMessage(const QString &text);
    void appendAssistantMessage(const QString &text);
    void updateAssistantDisplay();
    QString markdownToHtml(const QString &text);

    // UI widgets
    QComboBox *modelCombo_;
    QTextBrowser *chatView_;
    QLineEdit *input_;
    QPushButton *sendButton_;
    QPushButton *stopButton_;

    QNetworkAccessManager *network_;
    QString currentModel_;

    // Streaming state
    QNetworkReply *currentReply_ = nullptr;
    QByteArray buffer_;
    QString currentAssistantContent_;
    QTextCursor assistantCursor_;
};