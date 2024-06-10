INSERT INTO tHistory
    (date, nzbName, size, avgSpeed, archiveName, archivePass,
    groups, poster, packingPath, nzbFilePath, nbFiles, done)
VALUES
    ('2024-06-06 22:43:21', 'file1.nzb', '2.00 MB', '198.35 kB/s', 'file1',
    '24T2[l4z3MM8e1E8$o1]p^1gv', 'alt.binaries.superman', 'tFMveRgjbo6ZX@ngPost.com',
    '://db/resume_test/file1.nzb', '/tmp/resumed_file1.nzb', 6, 0);

INSERT INTO tUnfinishedFiles (job_id, filePath)
VALUES (1, '://db/resume_test/file1.nzb/file1.vol00+01.par2');

INSERT INTO tUnfinishedFiles (job_id, filePath)
VALUES (1, '://db/resume_test/file1.nzb/file1.vol01+01.par2');

INSERT INTO tUnfinishedFiles (job_id, filePath)
VALUES (1, '://db/resume_test/file1.nzb/file1.part3.rar');


