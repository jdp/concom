Concom
======

Concom is a program that provides an environment for people to explore and learn the ideas behind concatenative combinators. Concom is a symbolic environment, no computation is going on (unless you've gone on to encoding Church numerals), and it exists purely for people to learn. It is also a perfect companion to Brent Kerby's [The Theory of Concatenative Combinators](http://tunes.org/~iepos/joy.html), from which it takes its syntax.

Using
-----

Go through this familiar process to get a REPL:

    $ git clone git://github.com:jdp/concom.git
    $ cd concom
    $ make
    $ ./concom

And that's that. Error handling might be a bit shaky for now but that's getting fixed. If you don't want a REPL, you can execute a file just as easily:

    $ ./concom filename

Syntax
------

Concom only has 9 primitive words, including the first 8 presented in TTOCC. The available words at startup are `swap`, `dup`, `zap`, `empty`, `unit`, `cat`, `cons`, `i`, and `dip`. The non-combinator primitives are `show`, which shows the stack, and `exit`, which exits the interpreter. Comments begin with the octothorpe (#).

If for whatever reason you find yourself needing to define your own words, the familiar Forth/Factor syntax is available. A word definition starts with a colon (:), the name of the word, the body of the word, and a terminating semicolon (;) Here's an example definition of the `k` combinator:

    # [B] [A] k == A
    : k [zap] dip i ;

The sections of code between the square brackets ([, ]) are called quotations. They are complete and valid Concom programs, and for all intents and purposes they are anonymous functions.

Reference
---------

`swap ( [B] [A] -- [A] [B] )`

The `swap` combinator swaps the top two elements on the stack.

`dup ( [A] -- [A] [A] )`

The `dup` combinator duplicates the element on top of the stack, resulting in two identical elements sitting atop the stack.

`zap ( [A] -- )`

The `zap` combinator removes the top element from the stack.

`empty ( ... [C] [B] [A] -- )`

The `empty` combinator clears the stack. It is not a true combinator, just a convenience.

`cat ( [B] [A] -- [B A] )`

The `cat` combinator concatenates two quotations.

`unit ( [A] -- [[A]] )`

The `unit` combinator wraps the top element of the stack in a quotation.

`cons ( [B] [A] -- [[B] A] )`

The `cons` combinator acts a bit like the `cat` combinator, but the second value to come off the stack does not get dequoted.

`i ( [A] -- A )`

The `i` combinator dequotes (evaluates) the quoted program atop the stack.

`dip ( [B] [A] -- A [B] )`

The `dip` combinator first stores the item under the top of the stack, and then dequotes the item atop the stack. After that, it restores the item it saved to the top of the stack.

Credits
-------

Inspiration & Code
[The Theory of Concatenative Combinators](http://tunes.org/~iepos/joy.html)
[Picol Tcl Interpreter](http://antirez.com/page/picol.html)

Copyright &copy; 2010 Justin Poliey
[justinpoliey.com](http://justinpoliey.com)
[@justinpoliey](http://twitter.com/justinpoliey)
[Exploring Concatenative Combinators](http://tumble.justinpoliey.com/post/314824535/exploring-concatenative-combinators) &mdash; the blog post

