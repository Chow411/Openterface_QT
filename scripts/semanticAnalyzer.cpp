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
#include "keyboardMouse.h"

SemanticAnalyzer::SemanticAnalyzer(MouseManager* mouseManager, KeyboardManager* keyboardManager) 
    : mouseManager(mouseManager), keyboardManager(keyboardManager) {
    if (!mouseManager) {
        qDebug() << "MouseManager is not initialized!";
    }
    if (!keyboardManager) {
        qDebug() << "KeyboardManager is not initialized!";
    }
}

void SemanticAnalyzer::analyze(const ASTNode* node) {
    if (!node) {
        qDebug() << "Received null node in analyze method.";
        return;
    }
    
    switch (node->getType()) {
        case ASTNodeType::StatementList:
            // Process each statement in the list
            for (const auto& child : node->getChildren()) {
                qDebug() << "Analyzing child node.";
                analyze(child.get());
                resetParameters(); // Reset after each statement
            }
            break;
            
        case ASTNodeType::CommandStatement:
            qDebug() << "Analyzing command statement.";
            analyzeCommandStetement(static_cast<const CommandStatementNode*>(node));
            break;
            
        default:
            // Process any child nodes
            for (const auto& child : node->getChildren()) {
                qDebug() << "Analyzing default child node.";
                analyze(child.get());
            }
            break;
    }
}

void SemanticAnalyzer::resetParameters() {
    if (mouseManager) {
        // Reset mouse manager state
        mouseManager->reset();
        qDebug() << "Reset parameters for next statement";
    } else {
        qDebug() << "MouseManager is not available for reset!";
    }
}

void SemanticAnalyzer::analyzeCommandStetement(const CommandStatementNode* node){
    QString commandName = node->getCommandName();
    
    if(commandName == "Click"){
        analyzeClickStatement(node);
    }
    if(commandName == "Send"){
        qDebug() << "send command";
        analyzeSendStatement(node);
    }
}

void SemanticAnalyzer::analyzeSendStatement(const CommandStatementNode* node) {
    const auto& options = node->getOptions();
    // Map for special keys
    
    
    if (options.empty()) {
        qDebug() << "No coordinates provided for Send command";
        return;
    }
    QString tmpKeys;
    for (const auto& token : options){
        qDebug() << token;
        if (token != "\"") tmpKeys.append(token);
    }
    bool hasBrace = tmpKeys.contains("{");
    qDebug() << "tmp key:" << tmpKeys;
    if (!hasBrace){
        // Iterate through each character in the text to send
        for (const QString& ch : tmpKeys) {
            qDebug() << "Sent key:" << ch;
            if (specialKeys.contains(ch)) {
                int keyCode = specialKeys[ch];
                // Send the special key
                qDebug() << "keycode: " << keyCode; 
                keyboardManager->handleKeyboardAction(keyCode, 0, true); // Key down
                keyboardManager->handleKeyboardAction(keyCode, 0, false); // Key up
                
            } else {
                // Check if the character is a regular key
                if (AHKmapping.contains(ch)) {
                    int keyCode = AHKmapping.value(ch);
                    // Send the regular key
                    qDebug() << "keycode: " << keyCode; 
                    keyboardManager->handleKeyboardAction(keyCode, 0, true); // Key down
                    keyboardManager->handleKeyboardAction(keyCode, 0, false); // Key up
                }
            }
        }
    }

}

void SemanticAnalyzer::analyzeClickStatement(const CommandStatementNode* node) {
    const auto& options = node->getOptions();
    if (options.empty()) {
        qDebug() << "No coordinates provided for Click command";
        return;
    }
    
    // Parse coordinates and mouse button from options
    QPoint coords = parseCoordinates(options);
    int mouseButton = parseMouseButton(options);  // This will be fresh for each statement

    qDebug() << "Executing click at:" << coords.x() << "," << coords.y() 
             << "with button:" << mouseButton;

    try {
        mouseManager->handleAbsoluteMouseAction(coords.x(), coords.y(), mouseButton, 0);
    } catch (const std::exception& e) {
        qDebug() << "Exception caught in handleAbsoluteMouseAction:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception caught in handleAbsoluteMouseAction.";
    }
}

QPoint SemanticAnalyzer::parseCoordinates(const std::vector<std::string>& options) {
    if (options.empty()) {
        qDebug() << "No coordinate components";
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
        qDebug() << "Invalid coordinate format, using defaults";
        return QPoint(0, 0);
    }
    
    qDebug() << "Parsed coordinates:" << x << "," << y;
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
    
    // qDebug() << "Parsed mouse button:" << mouseButton << "from options:" << options;

    return mouseButton;
}
