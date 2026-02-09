#pragma once
#include <string>
#include <vector>

// המבנה הזה ייצג כתבה אחת בודדת
// אנחנו משתמשים ב-std::string כפי שנדרש בעבודה עם STL
struct NewsItem {
    std::string title;       // כותרת
    std::string author;      // שם הכתב
    std::string content;     // תוכן הכתבה
    std::string date;        // תאריך
    std::string time;        // שעה
    std::string readMoreUrl; // קישור לכתבה המלאה
    std::string imageUrl;    // קישור לתמונה
};