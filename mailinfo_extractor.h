/**
 * @file mailinfo_extractor.h
 */
#ifndef SM__MAILINFO_EXTRACTOR_H
#define SM__MAILINFO_EXTRACTOR_H


/**
 * @brief Removes and frees all entries from the lazy list.
 *
 * @note needs to be called at the context of the main thread.
 */
void lazy_clean_list(void);

/**
 * @brief Loads the excerpt of the given mail of the current folder
 * in a lazy fashion.
 *
 * In order to load the excerpt in a lazy fashion this functions spawns a
 * sub thread.
 *
 * @param mail
 * @return whether successful
 */
int simplemail_get_mail_info_excerpt_lazy(struct mail_info *mail);

/**
 * Cleanup resouces used by the mailinfo extractor.
 */
void cleanup_mailinfo_extractor(void);

#endif /* SM__MAILINFO_EXTRACTOR_H */
