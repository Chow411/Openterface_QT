#ifndef SCRIPTEDITOR_H
#define SCRIPTEDITOR_H

#include <QTextEdit>
#include <QWidget>

class LineNumberArea;

class ScriptEditor : public QTextEdit
{
    Q_OBJECT

public:
    explicit ScriptEditor(QWidget *parent = nullptr);
    ~ScriptEditor();

    void setText(const QString &text);          // To set HTML or plain text
    int lineNumberAreaWidth();                  // Public for LineNumberArea
    void lineNumberAreaPaintEvent(QPaintEvent *event);  // Public for LineNumberArea
    void highlightLine(int lineNumber);
    void resetHighlightLine(int lineNumber);

private slots:
    void updateLineNumberAreaWidth();           // No parameter needed

private:
    LineNumberArea *lineNumberArea;

    void updateLineNumberArea(const QRect &rect, int dy);
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;  // Override to handle scrolling
    
};

class LineNumberArea : public QWidget
{
public:
    LineNumberArea(ScriptEditor *editor) : QWidget(editor), scriptEditor(editor) {}
    QSize sizeHint() const override { return QSize(scriptEditor->lineNumberAreaWidth(), 0); }

protected:
    void paintEvent(QPaintEvent *event) override { scriptEditor->lineNumberAreaPaintEvent(event); }

private:
    ScriptEditor *scriptEditor;
};

#endif // SCRIPTEDITOR_H
