

function setLoaded ()
{
  document.hasLoaded = true;
}


function getContentName ()
{
  var name;
  if ((name = document.select_contentName)) {
    return name;
  } else {
    return URLtoName (document.location.href);
  }
}


function contentLoaded ()
{
  setLoaded ();
  if (top.navigateFrame) {
    if (top.navigateFrame.document.hasLoaded) {
      top.navigateFrame.setSelectionFromContent ();
    }
  }
}


function navigationLoaded ()
{
  setLoaded ();
  if (top.navigateFrame) {
    if (top.contentFrame.document.hasLoaded) {
       setSelectionFromContent ();
    }
  }
}


function setSelectionFromContent ()
{
  if (top.contentFrame) {
    setSelection (top.contentFrame.getContentName ());
    top.contentFrame.focus ();
  }
}

