$dvi_previewer = 'start xdvi -watchfile 1.5';
$ps_previewer  = 'start gv --watch';
$pdf_previewer = 'start evince';

# Generate pdf using xelatex
$pdf_mode = 5;
$xelatex = 'xelatex -synctex=1 --interaction=nonstopmode -file-line-error %O %S 1> /dev/null';

# Use bibtex if a .bib file exists
$bibtex_use = 1.5;

# Also remove pdfsync files on clean
$clean_ext = 'pdfsync synctex.gz xdv';

$postscript_mode = $dvi_mode = 0;