#include "scriptEditor.h"
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>
#include <QScrollBar>
#include <QGuiApplication>

ScriptEditor::ScriptEditor(QWidget *parent)
    : QTextEdit(parent)
{
    setFont(QFont("Courier", 10));
    setLineWrapMode(QTextEdit::NoWrap);

    lineNumberArea = new LineNumberArea(this);
    connect(document(), &QTextDocument::contentsChanged, this, &ScriptEditor::updateLineNumberAreaWidth);
    updateLineNumberAreaWidth();
}

ScriptEditor::~ScriptEditor()
{
}

void ScriptEditor::setText(const QString &text)
{
    setHtml(text);
    updateLineNumberAreaWidth();
    lineNumberArea->update();
}

int ScriptEditor::lineNumberAreaWidth()
{
    QString htmlContent = toHtml();
    
    int brCount = htmlContent.count("<br", Qt::CaseInsensitive);
    
    int lineCount = brCount + 1;

    int digits = 1;
    int max = qMax(1, lineCount);
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void ScriptEditor::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void ScriptEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth();
}

void ScriptEditor::resizeEvent(QResizeEvent *e)
{
    QTextEdit::resizeEvent(e);
    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void ScriptEditor::scrollContentsBy(int dx, int dy)
{
    QTextEdit::scrollContentsBy(dx, dy);
    if (dy != 0) {
        updateLineNumberArea(QRect(), dy);
    }
}

void ScriptEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    
    QPalette palette = QGuiApplication::palette();
    QColor bgColor = palette.color(QPalette::Window); 
    painter.fillRect(event->rect(), bgColor);

    QString plainText = toPlainText();
    QStringList lines = plainText.split('\n');
    int scrollOffset = verticalScrollBar()->value();
    int lineHeight = fontMetrics().height();

    QColor textColor = palette.color(QPalette::WindowText);
    painter.setPen(textColor);

    for (int i = 0; i < lines.size(); ++i) {
        int top = i * lineHeight - scrollOffset + 4;
        int bottom = top + lineHeight;

        if (top <= event->rect().bottom() && bottom >= event->rect().top()) {
            QString number = QString::number(i + 1);
            painter.drawText(0, top, lineNumberArea->width(), lineHeight, Qt::AlignRight, number);
        }
    }
}