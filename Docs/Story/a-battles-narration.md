# A battle's narration

Many of the second disc's battles open with a narration: pages told over the battle before it begins. Where a stretch
of your own has just shown what the first pages tell, the narration should begin later, and perhaps with a page of
your own that bridges from your scene. A document that begins `battle event N` does that, for battle event N (the
number a formation names).

Example: `Content/Story/disc2/rematch-narration.txt`. The lead-in above the forest has already told the meeting with
Ramsus (it shows the narration's first page itself, in Fei's cockpit, by naming the disc's own page), so the battle's
narration begins at its second page, after a page of the port's own:

```
battle event 19
message 0 from page 2 after
{
I could not shake him.{wait 1E}
   Driven from the sky,
      we came down in
         the forest...
}
```

- `message K from page P`: message K begins at its page P.
- `after` and a text between braces: these pages are told first, then the disc's own from page P on.

What you tell first is typed in the message's own window: its width and lines as the battle's table gives them (here
24 plain characters a line, 4 lines). A page that does not fit leaves the document out, and the tests fail on it.

The disc's words are never in the document: only where they begin and what is told before them.

## Telling a page of the disc's elsewhere

A map's document can name one of a battle's messages, or one page of it, instead of carrying its words:

```
message Met from event 19 message 0 page 1
```

`Content/Story/disc2/rematch-lead-in.txt` tells the meeting this way in the sky, in the disc's own words, read from your
disc as the map loads.
