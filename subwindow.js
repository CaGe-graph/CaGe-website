
top.document.subwindow = null;


function closeSubwindow ()
{
  if (top.document.subwindow != null)
    if (! top.document.subwindow.closed)
      top.document.subwindow.close ();
}

function openSubwindow (url, target, width, height, adjustable)
{
  closeSubwindow ();
  top.document.subwindow = window.open (url, target ? target : '', 'width=' + width + ',height=' + height + (adjustable ? ',scrollbars=1,resizable=1' : ''));
  top.document.subwindow.focus ();
}

function showSubwindow ()
{
  top.document.subwindow.document.close ();
  top.document.subwindow.focus ();
}

function imageWindow (link, width, height, title)
{
  var imageHTML =
'<html>\n' +
'<head>\n' +
'<title> &nbsp;' + title + '&nbsp; </title>\n' +
'</head>\n' +
'<body style="margin: 0; padding: 0;" onLoad="var image = document.getElementById(\'image\'); window.innerWidth = image.width; window.innerHeight = image.height;">\n' +
'<img id="image" src="' + link.href + '" alt="[Image]" title="' + title + '  [click to close]" style="cursor: crosshair;" onClick="window.close();" />\n' +
'</body>\n' +
'</html>\n';
  window.focus ();
  openSubwindow ('', link.target, width, height, false);
  var imageDocument = top.document.subwindow.document;
  imageDocument.open();
  imageDocument.writeln (imageHTML);
  imageDocument.close();
  showSubwindow ();
  return false;
}


function documentWindow (link, width, height)
{
  openSubwindow (link.href, link.target, width+30, height+30, false);
  showSubwindow ();
  return false;
}

