// Putting a program in the URL, after the "#", so a link opens with that code.
// The hash never reaches the server, so the program stays in the browser.
//
//   #code=<base64url of the deflate-compressed UTF-8 source>
//   #src=<base64url of the plain UTF-8 source>   (older browsers, no CompressionStream)

function toBase64Url(bytes) {
  let binary = "";
  for (let i = 0; i < bytes.length; i += 0x8000) {
    binary += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
  }
  return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

function fromBase64Url(text) {
  const base64 = text.replace(/-/g, "+").replace(/_/g, "/");
  const binary = atob(base64 + "===".slice((base64.length + 3) % 4));
  return Uint8Array.from(binary, (c) => c.charCodeAt(0));
}

async function pipe(bytes, transform) {
  const stream = new Blob([bytes]).stream().pipeThrough(transform);
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

export async function encodeSource(source) {
  const bytes = new TextEncoder().encode(source);
  if (typeof CompressionStream === "function") {
    return "code=" + toBase64Url(await pipe(bytes, new CompressionStream("deflate-raw")));
  }
  return "src=" + toBase64Url(bytes);
}

// Returns the program in a hash like "#code=...", or null if there is none.
// Throws if the hash is damaged (cut short when copied, for example).
export async function decodeSource(hash) {
  const params = new URLSearchParams(hash.replace(/^#/, ""));
  if (params.has("code")) {
    const bytes = await pipe(fromBase64Url(params.get("code")), new DecompressionStream("deflate-raw"));
    return new TextDecoder().decode(bytes);
  }
  if (params.has("src")) {
    return new TextDecoder().decode(fromBase64Url(params.get("src")));
  }
  return null;
}
