

document.select_Timeout = new Array;
document.select_Status = new Array;
document.select_selection = new Object;
document.select_selection.linkName = null;
document.select_linkimages = new Array;


function getLinkName (link)
{
  var name;
  if ((name = link.name)) {
    return name;
  } else {
    return URLtoName (link.href);
  }
}


function URLtoName (url)
{
  var p, name;
  name = url;
  p = name.lastIndexOf ("/");
  if (p >= 0) name = name.substring (p+1, name.length);
  p = name.indexOf (".");
  if (p >= 0) name = name.substring (0, p);
  return name;
}


function getImageCount (linkName)
{
  var i, imgName, result;
  if ((result = document.select_linkimages [linkName])) return result;
  for (i = 1; ; ++i)
  {
    imgName = linkName + "_" + i;
    if (! (document [imgName] && document [imgName].src)) {
      break;
    }
  }
  result = i == 1 ? null : i - 1;
  document.select_linkimages [linkName] = result;
  return result;
}


function setFocused (link, n)
{
  var linkName;
  linkName = getLinkName (link);
  document.select_linkimages [linkName] = n;
  if (document.select_selection.linkName == linkName) {
    return true;
  } else {
    setStatus ("focused", linkName, true, n);
    return false;
  }
}


function setPassive (link, n)
{
  var linkName;
  linkName = getLinkName (link);
  document.select_linkimages [linkName] = n;
  resetStatus (linkName, n);
}


function setClicked (link, n)
{
  window.focus ();
  var linkName;
  linkName = getLinkName (link);
  document.select_linkimages [linkName] = n;
  if (document.select_selection.linkName == linkName) {
    return false;
  } else {
    setSelection (linkName);
    return true;
  }
}


function setSelection (selectionName)
{
  unsetSelection ();
  parseSelection (selectionName);
  var l = document.select_selection.element.length - 1;
  for (var i = 0; i < l; ++i)
  {
    var imageCount;
    imageCount = getImageCount (document.select_selection.element [i]);
    setStatus ("focused", document.select_selection.element [i], false, imageCount);
    if (imageCount) {
      setStatus ("semifocused", document.select_selection.element [i], false, 0);
    }
  }
  setStatus ("selected", document.select_selection.element [l], false, getImageCount (document.select_selection.element [l]));
}


function unsetSelection ()
{
  if (! document.select_selection) return;
  if (! document.select_selection.linkName) return;
  for (var i = 0; i < document.select_selection.element.length; ++i)
  {
    setStatus ("passive", document.select_selection.element [i], false, getImageCount (document.select_selection.element [i]));
  }
  document.select_selection.linkName = null;
}


function parseSelection (selectionName)
{
  var p1, p2, l, i, element;
  document.select_selection.element = new Array;
  i = 0;
  l = selectionName.length;
  p2 = -1;
  while ((p1 = p2 + 1) < l)
  {
    p2 = selectionName.substring (p1).indexOf ("/") + p1;
    if (p2 < p1) p2 = l;
    element = selectionName.substring (p1, p2);
    document.select_selection.element [i++] = element;
  }
  document.select_selection.linkName = element;
}


function setStatus (newStatus, name, reset, n)
{
  if (setStatus.arguments.length < 4 || n == null) return setStatus1 (newStatus, reset, name);
  var i;
  for (i = 0; i <= n; ++i)
  {
    setStatus1 (newStatus, reset, name + "_" + i);
  }
  return true;
}


function resetStatus (name, n)
{
  if (resetStatus.arguments.length < 2 || n == null) return resetStatus1 (name);
  var i;
  for (i = 0; i <= n; ++i)
  {
    resetStatus1 (name + "_" + i);
  }
  return true;
}


function setStatusNames (newStatus, reset, names)
{
  var i, n, namesArray;
  namesArray = setStatusNames.arguments;
  n = namesArray.length;
  for (i = 2; i < n; ++i)
  {
    setStatus1 (newStatus, reset, namesArray [i]);
  }
  return true;
}


function setStatus1 (newStatus, reset, imgName)
{
  var img, id;
  img = document [imgName];
  if ((! img) || (! img.src)) return true;
  id = document.select_Timeout [imgName];
  if (id) clearTimeout (id);
  cmd = "setImgStatus ('" + newStatus + "', '" + imgName + "');"
  if (reset) {
    cmd += " document.select_Timeout ['" + imgName + "'] = setTimeout (\"resetStatus ('" + imgName + "');\", 4000);";
  } else {
    document.select_Status [imgName] = newStatus;
  }
  document.select_Timeout [imgName] = setTimeout (cmd, 10);
  return true;
}


function resetStatus1 (imgName)
{
  var oldStatus;
  oldStatus = document.select_Status [imgName];
  if (! oldStatus) oldStatus = "passive";
  setStatus1 (oldStatus, false, imgName);
}


function setImgStatus (status, imgName)
{
  var img, p1, src;
  img = document [imgName];
  src = img.src;
  p1 = src.lastIndexOf (".");
  if (p1 < 0) p1 = src.length;
  p2 = src.substring (0, p1).lastIndexOf ("-");
  if (p2 < 0) p2 = src.length;
  src = src.substring (0, p2) + "-" + status + src.substring (p1, src.length);
  img.src = src;
  return true;
}

