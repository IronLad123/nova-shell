#ifndef HISTORY_H
#define HISTORY_H

void        history_init(void);
void        history_add(const char *cmd);
void        history_print(void);
void        history_save(void);
const char *history_get(int n);           /* 1-based; NULL if out of range */
int         history_count_get(void);

#endif /* HISTORY_H */
