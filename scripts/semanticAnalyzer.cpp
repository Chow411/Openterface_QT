/*
* ========================================================================== *
*                                                                            *
*    This file is part of the Openterface Mini KVM App QT version            *
*                                                                            *
*    Copyright (C) 2024   <info@openterface.com>                             *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation version 3.                                 *
*                                                                            *
*    This program is distributed in the hope that it will be useful, but     *
*    WITHOUT ANY WARRANTY; without even the implied warranty of              *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU        *
*    General Public License for more details.                                *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see <http://www.gnu.org/licenses/>.    *
*                                                                            *
* ========================================================================== *
*/ 


#include "semanticAnalyzer.h"
#include <stdexcept>
#include <QDebug>
#include <QString>
#include "AHKKeyboard.h"
#include "KeyboardMouse.h"


Q_LOGGING_CATEGORY(log_script, "opf.scripts")

SemanticAnalyzer::SemanticAnalyzer(MouseManager* mouseManager, KeyboardMouse* keyboardMouse)
    : mouseManager(mouseManager), keyboardMouse(keyboardMouse) {
    if (!mouseManager) {
        qDebug(log_script) << "MouseManager is not initialized!";
    }

}

void SemanticAnalyzer::analyze(const ASTNode* node) {
    if (!node) {
        qDebug(log_script) << "Received null node in analyze method.";
        return;
    }
    
    switch (node->getType()) {
        case ASTNodeType::StatementList:
            // Process each statement in the list
            for (const auto& child : node->getChildren()) {
                qDebug(log_script) << "Analyzing child node.";
                analyze(child.get());
                resetParameters(); // Reset after each statement
            }
            break;
            
        case ASTNodeType::CommandStatement:
            qDebug(log_script) << "Analyzing command statement.";
            analyzeCommandStetement(static_cast<const CommandStatementNode*>(node));
            break;
            
        default:
            // Process any child nodes
            for (const auto& child : node->getChildren()) {
                qDebug(log_script) << "Analyzing default child node.";
                analyze(child.get());
            }
            break;
    }
}

void SemanticAnalyzer::resetParameters() {
    if (mouseManager) {
        // Reset mouse manager state
        mouseManager->reset();
        qDebug(log_script) << "Reset parameters for next statement";
    } else {
        qDebug(log_script) << "MouseManager is not available for reset!";
    }
}

void SemanticAnalyzer::analyzeCommandStetement(const CommandStatementNode* node){
    QString commandName = node->getCommandName();
    
    if(commandName == "Click"){
        analyzeClickStatement(node);
    }
    if(commandName == "Send"){
        analyzeSendStatement(node);
    }
    if(commandName == "Sleep"){
        analyzeSleepStatement(node);
    }
    if(commandName == "SetCapsLockState"){
        analyzeLockState(node, "CapsLock", &KeyboardMouse::getCapsLockState_);
    }
    if(commandName == "SetNumLockState"){
        analyzeLockState(node, "NumLock", &KeyboardMouse::getNumLockState_);
    }
    if(commandName == "SetScrollLockState"){
        analyzeLockState(node, "ScrollLcok", &KeyboardMouse::getScrollLockState_);
    }
}

void SemanticAnalyzer::analyzeLockState(const CommandStatementNode* node, const QString& keyName, bool (KeyboardMouse::*getStateFunc)()){
    const auto& options = node->getOptions();
    std::array<uint8_t, 6> general = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    if (options.empty()){
        qCDebug(log_script) << "Please enter parameters.";
        return;
    }
    QString tmpKeys;
    for (const auto& token : options){
        if (token != "\"") tmpKeys.append(QString::fromStdString(token));
    }
    tmpKeys.remove(' ');
    qCDebug(log_script) << tmpKeys;
    if (onRegex.match(tmpKeys).hasMatch()){
        qCDebug(log_script) << keyName << " on";
        keyboardMouse->updateNumCapsScrollLockState();
        if (!(keyboardMouse->*getStateFunc)()){
            general[0] = keydata.value(keyName);
            keyPacket pack(general);
            keyboardMouse->addKeyPacket(pack);
            keyboardMouse->keyboardSend();
        }
    }
    if (offRegex.match(tmpKeys).hasMatch()){
        qCDebug(log_script) << keyName << " off";
        keyboardMouse->updateNumCapsScrollLockState();
        if ((keyboardMouse->*getStateFunc)()) {
            general[0] = keydata.value(keyName);
            keyPacket pack(general);
            keyboardMouse->addKeyPacket(pack);
            keyboardMouse->keyboardSend();
        }
    }
}

void SemanticAnalyzer::analyzeSleepStatement(const CommandStatementNode* node){
    const auto& options = node->getOptions();

    if (options.empty()){
        qDebug(log_script) << "No sleep time set";
        return;
    }
    for (const auto& token : options){
        // Assuming the first option is the sleep time in milliseconds
        bool ok;
        int sleepTime = QString::fromStdString(token).toInt(&ok);
        
        if (!ok || sleepTime < 0) {
            continue; // Exit if the sleep time is invalid
        }else{
            qDebug(log_script) << "Sleeping for" << sleepTime << "milliseconds";
            QThread::msleep(sleepTime); // Introduce the delay
        }
    }
}

void SemanticAnalyzer::analyzeSendStatement(const CommandStatementNode* node) {
    const auto& options = node->getOptions();
    // Map for special keys
    
    if (options.empty()) {
        qDebug(log_script) << "No coordinates provided for Send command";
        return;
    }
    QString tmpKeys;
    for (const auto& token : options){
        qDebug(log_script) << QString::fromStdString(token);
        if (token != "\"") tmpKeys.append(QString::fromStdString(token));
    }
    int i = 0;
    // QRegularExpressionMatch embedClick = sendEmbedRegex(tmpKeys);
    QRegularExpression regex(R"((\{[^}]+\}|[^{]\w+))");
    QRegularExpressionMatchIterator matchIterator = regex.globalMatch(tmpKeys);
    std::vector<int> keys;
    while (i<tmpKeys.length()){
        const QChar& ch = tmpKeys[i];
        
        std::array<uint8_t, 6> general = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t control = 0x00;
        if (ch != '{' && !controldata.contains(ch)){
            general[0] = keydata.value(ch);
        } else if(ch  == '{'){
            qDebug(log_script) << "General data";
            extractKeyFromBrace(tmpKeys, i, general);
        } else if (controldata.contains(ch))
        {
            qDebug(log_script) << "control data";
            int index = 0;
            control = controldata.value(ch);
            for (int j = i+1; j < tmpKeys.length(); j++){
                if (tmpKeys[j] == '{'){
                    extractKeyFromBrace(tmpKeys, j, general, index);
                    index += 1;
                }else if(controldata.contains(tmpKeys[j])){
                    general[index] = keydata.value(tmpKeys[j]);
                    index += 1;
                }else{
                    general[index] = keydata.value(tmpKeys[j]);
                    i = j;
                    break;
                }
            }
        }
        keyPacket pack(general,control);
        keyboardMouse->addKeyPacket(pack);
        i++;
    }

    keyboardMouse->keyboardSend();
    // for (int i =0; i<keys.size(); i++){
    //     keyboardManager->handleKeyboardAction(keys[i], 0, true);
    //     keyboardManager->handleKeyboardAction(keys[i], 0, false);
    // }

}

void SemanticAnalyzer::extractClickParameters(const QString& statement){
    QRegularExpressionMatch match = clickRegex.match(statement);
    QString coords;
    if(match.hasMatch()){
        QString coords = match.captured(1).isEmpty() ? match.captured(2).trimmed() : match.captured(1).trimmed();
        QString button = match.captured(3).isEmpty() ? match.captured(4).trimmed() : match.captured(3).trimmed();
        QString count = match.captured(5).isEmpty() ? match.captured(6).trimmed() : match.captured(5).trimmed();
        QString DownOrUp = match.captured(7).isEmpty() ? match.captured(8).trimmed() : match.captured(7).trimmed();
        QString Relative = match.captured(9).isEmpty() ? match.captured(10).trimmed() : match.captured(9).trimmed();
    }
}

void SemanticAnalyzer::extractKeyFromBrace(const QString& tmpKeys, int& i, std::array<uint8_t, 6>& general, int genral_index){
    QString tmpkey;

    for (int j = i + 1; j < tmpKeys.length(); j++) {
        if (tmpKeys[j] != '}') {
            tmpkey.append(tmpKeys[j]);
        } else {
            general[genral_index] = keydata.value(tmpkey);
            i = j;
            break;
        }
    }
}

void SemanticAnalyzer::analyzeClickStatement(const CommandStatementNode* node) {
    const auto& options = node->getOptions();
    if (options.empty()) {
        qDebug(log_script) << "No coordinates provided for Click command";
        return;
    }
    
    // Parse coordinates and mouse button from options
    QPoint coords = parseCoordinates(options);
    int mouseButton = parseMouseButton(options);  // This will be fresh for each statement

    qDebug(log_script) << "Executing click at:" << coords.x() << "," << coords.y() 
             << "with button:" << mouseButton;

    try {
        mouseManager->handleAbsoluteMouseAction(coords.x(), coords.y(), mouseButton, 0);
    } catch (const std::exception& e) {
        qDebug(log_script) << "Exception caught in handleAbsoluteMouseAction:" << e.what();
    } catch (...) {
        qDebug(log_script) << "Unknown exception caught in handleAbsoluteMouseAction.";
    }
}

QPoint SemanticAnalyzer::parseCoordinates(const std::vector<std::string>& options) {
    if (options.empty()) {
        qDebug(log_script) << "No coordinate components";
        return QPoint(0, 0);
    }
    
    int x = 0, y = 0;
    bool foundComma = false;
    bool beforeComma = true;
    bool okX = false, okY = false;
    
    for (const auto& token : options) {
        if (token == ",") {
            foundComma = true;
            beforeComma = false;
            continue;
        }
        
        // Try to parse the token as a number
        bool ok = false;
        int value = QString::fromStdString(token).toInt(&ok);
        if (ok) {
            if (beforeComma) {
                x = value;
                okX = true;
            } else {
                y = value;
                okY = true;
            }
        }
    }
    
    if (!foundComma || (!okX && !okY)) {
        qDebug(log_script) << "Invalid coordinate format, using defaults";
        return QPoint(0, 0);
    }
    
    qDebug(log_script) << "Parsed coordinates:" << x << "," << y;
    return QPoint(x, y);
}

int SemanticAnalyzer::parseMouseButton(const std::vector<std::string>& options) {
    // Default to left click for each new statement 
    int mouseButton = Qt::LeftButton;
    
    // Look for button specification in options
    for (const auto& option : options) {
        QString opt = QString::fromStdString(option).toLower();  // Convert to lowercase for case-insensitive comparison
        if (opt == "right" || opt == "r") {
            mouseButton = Qt::RightButton;
            break;  // Exit once we find a button specification
        } else if (opt == "middle" || opt == "m") {
            mouseButton = Qt::MiddleButton;
            break;  // Exit once we find a button specification
        }
    }
    
    // qDebug(log_script) << "Parsed mouse button:" << mouseButton << "from options:" << options;

    return mouseButton;
}

void SemanticAnalyzer::analyzeMouseMove(const CommandStatementNode* node) {
    const auto& options = node->getOptions();
    if (options.empty()) {
        qDebug(log_script) << "No coordinates provided for MouseMove command";
        return;
    }
    
    // Parse coordinates and speed from options
    QPoint coords = parseCoordinates(options);
}
