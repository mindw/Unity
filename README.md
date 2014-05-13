Unity
=====

Simple Unit Testing for C

This is a heavily modified verion of unity for embedded systems:

* Does not use longjmp for error handling (modified from original).
* Does not yse any globals.
* Additional macros for test data creation and usage.
* The correct file name where the assert failed is reported (it may have failed in a file other
    than the test is implemented).
