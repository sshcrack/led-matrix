use anyhow::{anyhow, bail, Result};
use reqwest::{Method, Url};
use scraper::{Html, Selector};
use lazy_static::lazy_static;


pub const URL: &str = "https://pixeljoint.com/pixels/new_icons.asp?q=1";
pub const SEARCH_COOKIE: &str = "v=av=&dimo=%3C%3D&anim=&iso=&ob=search&dint=&pg=2&search=&tran=&colors=2&d=&colorso=%3E%3D&dim=128&owner=0; path=/";

lazy_static! {
    pub static ref PARSED_URL: Url = Url::parse(URL).unwrap();
}


pub fn append_base(path: &str) -> Result<String> {
    let base = PARSED_URL.host().ok_or(anyhow!("Could not parse url"))?;
    let res = format!("{}://{}{}", PARSED_URL.scheme() ,base, path);

    Ok(res)
}

pub async fn get_pixel_posts(page: usize) -> Result<Vec<(String, String)>> {
    let client = reqwest::Client::new();
    let req = client.request(Method::GET,URL)
        .query(&[("pg", &page.to_string())])
        .header("Cookie", SEARCH_COOKIE)
        .build()?;

    println!("Sending post request to {}", req.url().as_str());
    let resp = client.execute(req).await?;
    let resp = resp.text().await?;

    let document = Html::parse_document(&resp);
    let selector = Selector::parse(r#".imglink"#).unwrap();

    let mut out = Vec::new();
    //println!("{:?}", imgs);
    for a in document.select(&selector) {
        let post = a.attr("href");

        let mut img = a.children().filter_map(|e| {
            e.value().as_element().and_then(|e| {
                if e.name() != "img" {
                    return None;
                }

                e.attr("src")
            })
        });
        let src = img.next();
        if src.is_none() {
            continue;
        }

        let src = src.unwrap().to_string();
        if let Some(post) = post {
            out.push((append_base(post)?, src));
        }
    }

    Ok(out)
}

pub async fn get_image_link(post :&str) -> Result<String> {
    let resp = reqwest::get(post).await?;
    let resp = resp.text().await?;

    let document = Html::parse_document(&resp);
    let selector = Selector::parse(r#"#mainimg"#).unwrap();
    let image = document.select(&selector).next();
    if image.is_none() {
        bail!("Post not found");
    }

    let image = image.unwrap();
    let src = image.attr("src").ok_or(anyhow!("No src"))?;
    let src = append_base(src)?;

    return Ok(src)
}