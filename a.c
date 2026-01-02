#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#ifdef _WIN32
#include <windows.h>
void delay(int ms){ Sleep(ms); }
#else
#include <unistd.h>
void delay(int ms){ usleep(ms * 1000); }
#endif

typedef struct {
    int code;
    char name[60];
    int marks;
    int fullMarks;
    char grade[3];
    int isOptional;
} Subject;

typedef struct {
    char roll[20];
    char reg[20];
    Subject *subjects;
    int subjectCount;
    double gpa;
    int fetchComplete;
    int fetchSuccess;
} FetchData;

int getFullMarks(int code) {
    if (code == 101 || code == 107) return 200;
    if (code == 147 || code == 156) return 50;
    return 100;
}

int isOptionalSubject(int code) {
    if (code == 147 || code == 156) return 0;
    return 0;
}

double gradePoint(int marks, int full, int code) {
    if (code == 147 || code == 156) {
        if (marks >= 40) return 5.0;
        if (marks >= 35) return 4.0;
        if (marks >= 30) return 3.5;
        if (marks >= 25) return 3.0;
        if (marks >= 20) return 2.0;
        if (marks >= 17) return 1.0;
        return 0.0;
    }
    double percent = (marks * 100.0) / full;
    if(percent >= 80) return 5.0;
    if(percent >= 70) return 4.0;
    if(percent >= 60) return 3.5;
    if(percent >= 50) return 3.0;
    if(percent >= 40) return 2.0;
    if(percent >= 33) return 1.0;
    return 0.0;
}

void printSubjects(Subject subjects[], int n) {
    printf("Code   Subject                                   Marks   Grade   Percent\n");
    printf("-----------------------------------------------------------------------------\n");
    for(int i = 0; i < n; i++) {
        double percent = (subjects[i].marks * 100.0) / subjects[i].fullMarks;
        printf("%-6d %-45s %-7d %-7s %-7.2f%%\n",
               subjects[i].code,
               subjects[i].name,
               subjects[i].marks,
               subjects[i].grade,
               percent);
    }
    printf("-----------------------------------------------------------------------------\n");
}

void typeText(const char text[], int speed) {
    for(int i = 0; text[i] != '\0'; i++) {
        putchar(text[i]);
        fflush(stdout);
        delay(speed);
    }
}

void printSignature() {
    typeText("Sincerely,\n", 30);
    typeText("House Representative (MAR)\n", 30);
    typeText("IT Club\n", 30);
    typeText("Adamjee Cantonment College\n", 30);
}

char* extractBetween(const char *str, const char *start, const char *end) {
    char *p1 = strstr(str, start);
    if (!p1) return NULL;
    p1 += strlen(start);
    
    char *p2 = strstr(p1, end);
    if (!p2) return NULL;
    
    int len = p2 - p1;
    char *result = malloc(len + 1);
    strncpy(result, p1, len);
    result[len] = '\0';
    
    while(*result == ' ' || *result == '\n' || *result == '\r' || *result == '\t') {
        memmove(result, result + 1, strlen(result));
    }
    len = strlen(result);
    while(len > 0 && (result[len-1] == ' ' || result[len-1] == '\n' || result[len-1] == '\r' || result[len-1] == '\t')) {
        result[--len] = '\0';
    }
    
    return result;
}

void* fetchResultThread(void *arg) {
    FetchData *data = (FetchData*)arg;
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "curl -s 'https://result19.comillaboard.gov.bd/2025/individual/result_marks_details.php' "
        "-H 'content-type: application/x-www-form-urlencoded' "
        "--data-raw 'roll=%s&reg=%s' -o /tmp/ssc_result.html 2>/dev/null",
        data->roll, data->reg);
    
    int ret = system(cmd);
    if (ret != 0) {
        data->fetchSuccess = 0;
        data->fetchComplete = 1;
        return NULL;
    }
    
    FILE *fp = fopen("/tmp/ssc_result.html", "r");
    if (!fp) {
        data->fetchSuccess = 0;
        data->fetchComplete = 1;
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *html = malloc(fsize + 1);
    fread(html, 1, fsize, fp);
    fclose(fp);
    html[fsize] = '\0';

    char *gpa_marker = "<td>GPA</td>";
    char *gpa_ptr = strstr(html, gpa_marker);
    if (gpa_ptr) {
        gpa_ptr += strlen(gpa_marker);
        char *value_td = "<td class=\"cap_lt txt_bold\">";
        char *value_ptr = strstr(gpa_ptr, value_td);
        if (value_ptr) {
            value_ptr += strlen(value_td);
            sscanf(value_ptr, "%lf", &data->gpa);
        }
    }
    
    int maxSubjects = 20;
    data->subjects = malloc(sizeof(Subject) * maxSubjects);
    data->subjectCount = 0;
    
    char *ptr = html;
    while (data->subjectCount < maxSubjects) {
        ptr = strstr(ptr, "<td class=\"bg_grey\">");
        if (!ptr) break;
        ptr += strlen("<td class=\"bg_grey\">");
        
        int code;
        if (sscanf(ptr, "%d", &code) != 1) break;
        data->subjects[data->subjectCount].code = code;
        data->subjects[data->subjectCount].isOptional = isOptionalSubject(code);
        
        ptr = strstr(ptr, "<td class=\"bg_grey cap_lt\">");
        if (!ptr) break;
        ptr += strlen("<td class=\"bg_grey cap_lt\">");
        
        char *endTag = strstr(ptr, "</td>");
        if (!endTag) break;
        
        int nameLen = endTag - ptr;
        if (nameLen > 59) nameLen = 59;
        strncpy(data->subjects[data->subjectCount].name, ptr, nameLen);
        data->subjects[data->subjectCount].name[nameLen] = '\0';
        
        char *name = data->subjects[data->subjectCount].name;
        while(*name == ' ' || *name == '\n') name++;
        memmove(data->subjects[data->subjectCount].name, name, strlen(name) + 1);
        int len = strlen(data->subjects[data->subjectCount].name);
        while(len > 0 && (data->subjects[data->subjectCount].name[len-1] == ' ' || data->subjects[data->subjectCount].name[len-1] == '\n')) {
            data->subjects[data->subjectCount].name[--len] = '\0';
        }
        
        ptr = strstr(ptr, "<td class=\"bg_grey cap_lt\">");
        if (!ptr) break;
        ptr += strlen("<td class=\"bg_grey cap_lt\">");
        
        int marks;
        char grade[4];
        if (sscanf(ptr, "%d=%2s", &marks, grade) == 2) {
            data->subjects[data->subjectCount].marks = marks;
            data->subjects[data->subjectCount].fullMarks = getFullMarks(code);
            strcpy(data->subjects[data->subjectCount].grade, grade);
            data->subjectCount++;
        }
        
        ptr = strstr(ptr, "</td>");
        if (!ptr) break;
    }
    
    free(html);
    remove("/tmp/ssc_result.html");
    
    data->fetchSuccess = (data->subjectCount > 0) ? 1 : 0;
    data->fetchComplete = 1;
    
    return NULL;
}

int checkCurlInstalled() {
    int ret = system("which curl > /dev/null 2>&1");
    return (ret == 0);
}

void installCurl() {
    printf("\n========================================\n");
    printf("curl is not installed on your system.\n");
    printf("========================================\n\n");
    printf("To install curl, run ONE of these commands:\n\n");
    printf("Ubuntu/Debian:\n");
    printf("  sudo apt-get update && sudo apt-get install -y curl\n\n");
    printf("Fedora/RHEL/CentOS:\n");
    printf("  sudo dnf install -y curl\n\n");
    printf("Arch Linux:\n");
    printf("  sudo pacman -S curl\n\n");
    printf("macOS:\n");
    printf("  brew install curl\n\n");
    printf("After installing, run this program again.\n");
    printf("========================================\n");
}

int main(int argc, char *argv[]) {
    char roll[20] = "152205";
    char reg[20] = "2211210980";
    if (!checkCurlInstalled()) {
        installCurl();
        return 1;
    }
    
    if (argc >= 2) {
        strncpy(roll, argv[1], 19);
        roll[19] = '\0';
    }
    if (argc >= 3) {
        strncpy(reg, argv[2], 19);
        reg[19] = '\0';
    }
    
    FetchData fetchData;
    strcpy(fetchData.roll, roll);
    strcpy(fetchData.reg, reg);
    fetchData.subjects = NULL;
    fetchData.subjectCount = 0;
    fetchData.gpa = 0.0;
    fetchData.fetchComplete = 0;
    fetchData.fetchSuccess = 0;
    
    pthread_t fetchThread;
    pthread_create(&fetchThread, NULL, fetchResultThread, &fetchData);
    
    printf("=============================================\n");
    typeText("Student Name : AS Ayman\n", 30);
    typeText("SSC Examination Result\n", 30);
    printf("=============================================\n\n");
    
    if (!fetchData.fetchComplete) {
        printf("Waiting to fetch the result");
        fflush(stdout);
        
        const char *spinner = "|/-\\";
        int spinnerIdx = 0;
        
        while (!fetchData.fetchComplete) {
            printf("\b\b\b\b%c...", spinner[spinnerIdx]);
            fflush(stdout);
            spinnerIdx = (spinnerIdx + 1) % 4;
            delay(150);
        }
        printf("\b\b\b\b    \b\b\b\b");
        printf("\n\n");
    }
    
    pthread_join(fetchThread, NULL);
    
    if (!fetchData.fetchSuccess || fetchData.subjectCount == 0) {
        printf("Failed to fetch result!\n");
        printf("Please check your roll number and registration number.\n");
        return 1;
    }
    
    int totalMarks = 0, totalFull = 0;
    
    for(int i = 0; i < fetchData.subjectCount; i++) {
        totalMarks += fetchData.subjects[i].marks;
        totalFull += fetchData.subjects[i].fullMarks;
    }

    printSubjects(fetchData.subjects, fetchData.subjectCount);
    printf("Total Marks : %d / %d (%.2f%%)\n", totalMarks, totalFull, (totalMarks * 100.0) / totalFull);
    printf("Total GPA   : %.2f\n", fetchData.gpa);
    printf("=============================================\n\n");
    
    printSignature();
    printf("\n=============================================\n");
    
    free(fetchData.subjects);
    return 0;
}
