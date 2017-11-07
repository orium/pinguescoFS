title=report
flags=-file-line-error -halt-on-error -interaction nonstopmode --shell-escape

all: $(title).tex
	pdflatex $(flags) $(title).tex
	bibtex $(title)
	pdflatex $(flags) $(title).tex
	pdflatex $(flags) $(title).tex

.PHONY: clean

clean:
	rm -f *.dvi *.aux *.log *.toc *.out *.bbl *.blg \
	      *.idx *.lof *.tdo # $(title).pdf
