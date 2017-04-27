.adoc.html:
	@mkdir -p $(top_srcdir)/doc/output
	asciidoc -a data-uri -b html5  -a icons -a toc2  -a max-width=55em --out-file=$@ $<
	mv $@ $(top_srcdir)/doc/output/$@
