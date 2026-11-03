-- Full-text index for comment search (offensive/content/search.inc,
-- core_searchcomments).  Replaces the Xapian index that
-- admin/commentIndexer.php built; Xapian has no PHP 8 bindings packaged for
-- Debian, so that index can't be read or written any more.
--
-- Safe to run before deploying the code.  Until it has run, comment search
-- fails with "Can't find FULLTEXT index matching the column list".
--
-- Not an in-place change: the first FULLTEXT index on an InnoDB table rebuilds
-- the table to add a hidden FTS_DOC_ID column, and MariaDB holds a shared lock
-- for the whole build, so comment writes block until it finishes.  On
-- production (2.6 GB, 19M rows) expect it to take a while; run it when the
-- site is quiet.  As with 2026-10-21.sql, point innodb_tmpdir at real disk
-- first, as a user with the FILE privilege:
--
--   SET GLOBAL innodb_tmpdir = '/var/tmp';
--
-- Afterwards offensive/data/comments.db* is unused and can be deleted.

ALTER TABLE offensive_comments ADD FULLTEXT INDEX comment_text (comment);
