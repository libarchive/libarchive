Thank you for helping us improve libarchive.
The following guidelines will help ensure your contribution gets prompt attention.

# Bugs and other Issues

If you encounter any problems with libarchive,
[please file an issue on our issue tracker](https://github.com/libarchive/libarchive/issues).

All bug reports should include the following information.  You can copy the text below directly into the issue tracker to get started:

```
Version of libarchive:
How you obtained it:  (build from source, pre-packaged binary, etc)
Operating system and version:
If you built from source, what compiler and/or IDE are you using:

Description of the problem you are seeing:

How the libarchive developers can reproduce your problem:
```

## Test Failures

If you see any test failures, please include the information above and also:

* Names of the tests that failed.

* Look for the .log files in the /tmp/libarchive_test_*date-and-time* directories.  (On Mac OS, look in $TMPDIR which is different than /tmp.)

Please paste the .log files you will find there directly into your report.

## Problems using libarchive in a program

If you are trying to write a program using libarchive, please include the information above and also:

```
It will help us if we can actually run the program.

If you have a sufficiently short program that shows the problem, you can either paste it into the report or [put it into a gist](https://gist.github.com).
```

## Libarchive produced incorrect output

Please try to make the corrupted output available to us.  You can upload it into a fresh github repository and provide a link in your issue report.

## Libarchive failed to read a particular input file

Note: If you can provide a very short input file that reproduces the problem, we can usually add that to our test suite.  This will ensure that the bug does not reappear in the future.

If the input file is very large and/or proprietary, please post an issue first, then follow-up with an email to libarchive-discuss@group.google.com so one of the libarchive developers can follow up with you.  In your email, please include a link to the issue report.



# Code Submissions

We welcome all code submissions.  But of course, some code submissions are easier for us to respond to than others. The best code submissions:

* Address a single issue.  There have been many cases where a simple fix to an obvious problem did not get handled for months because the patch that was provided also included an unrelated change that affected a controversial area of the code.

* Include detailed instructions for reproducing the problem you're fixing.  We do try to verify that a submission actually fixes a real problem.  If we can't reproduce the problem, it will take us longer to evaluate the fix.  For this reason, we encourage you to file an issue report first with details on reproducing the problem, then refer to that issue in your pull request.

* Includes a test case.  The libarchive Wiki has [detailed documentation for adding new test cases](https://github.com/libarchive/libarchive/wiki/LibarchiveAddingTest).

* Are provided via Github pull requests.  We welcome patches in almost any format, but github's pull request management makes it significantly easier for us to evaluate and test changes.
