const state={releases:[]};
const $=id=>document.getElementById(id);
const token=()=>sessionStorage.getItem("typego_admin_token")||"";
const s3Value=name=>sessionStorage.getItem(`typego_s3_${name}`)||"";
const escapeHTML=value=>String(value??"").replace(/[&<>"']/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));

async function api(path,options={}){
  const response=await fetch(path,{...options,headers:{"Authorization":`Bearer ${token()}`,"Content-Type":"application/json",...(options.headers||{})}});
  if(response.status===401){$("tokenDialog").showModal();throw new Error("请设置正确的管理 Token");}
  if(!response.ok){const body=await response.json().catch(()=>({}));throw new Error(body.error||`请求失败 (${response.status})`);}
  return response.status===204?null:response.json();
}

async function load(){
  try{
    const data=await api("/admin/api/v1/releases");
    state.releases=data.releases||[];
    render();
  }catch(error){message(error.message,true);}
}

function render(){
  const platform=$("platformFilter").value;
  const releases=state.releases.filter(item=>!platform||item.platform===platform);
  $("total").textContent=state.releases.length;
  $("published").textContent=state.releases.filter(item=>item.published).length;
  $("rollout").textContent=state.releases.filter(item=>item.published&&item.rollout_percent<100).length;
  $("empty").hidden=releases.length>0;
  $("releaseList").innerHTML=releases.map(item=>`<article class="release">
    <div><div class="release-head"><h3>${escapeHTML(item.version)}</h3><span class="badge">${escapeHTML(item.platform)} · ${escapeHTML(item.arch)}</span><span class="badge ${item.published?"":"off"}">${item.published?(item.rollout_percent<100?`灰度 ${item.rollout_percent}%`:"正式发布"):"未发布"}</span>${item.mandatory?'<span class="badge">强制更新</span>':""}</div>
    <p>${escapeHTML(item.changelog||"暂无更新日志")}</p><small>${escapeHTML(item.download_url)}</small></div>
    <div class="release-actions"><button class="secondary" data-edit="${escapeHTML(item.id)}">编辑</button><button class="danger" data-delete="${escapeHTML(item.id)}">删除</button></div>
  </article>`).join("");
}

function edit(id){
  const item=state.releases.find(value=>value.id===id);if(!item)return;
  $("id").value=item.id;$("platform").value=item.platform;$("arch").value=item.arch;$("version").value=item.version;
  $("downloadUrl").value=item.download_url;$("changelog").value=item.changelog;$("mandatory").checked=item.mandatory;
  $("publishedCheck").checked=item.published;$("rolloutPercent").value=item.rollout_percent;updateRange();
  $("formTitle").textContent=`编辑 ${item.version}`;window.scrollTo({top:0,behavior:"smooth"});
}

function reset(){
  $("releaseForm").reset();$("id").value="";$("arch").value="amd64";$("rolloutPercent").value=100;
  $("publishedCheck").checked=true;$("formTitle").textContent="新增版本";updateRange();message("");
}

function message(text,error=false){$("message").textContent=text;$("message").style.color=error?"#c33":"#238755";}
function uploadMessage(text,error=false){$("uploadMessage").textContent=text;$("uploadMessage").style.color=error?"#c33":"#238755";}
function updateRange(){$("rolloutOutput").textContent=`${$("rolloutPercent").value}%`;}

async function upload(){
  const file=$("s3File").files[0];
  if(!file){uploadMessage("请选择上传文件",true);return;}
  const accessKey=$("s3AccessKey").value.trim(),secretKey=$("s3SecretKey").value,bucket=$("s3Bucket").value.trim();
  if(!accessKey||!secretKey||!bucket){uploadMessage("请填写 Access Key、Secret Key 和 Bucket",true);return;}
  sessionStorage.setItem("typego_s3_access_key",accessKey);sessionStorage.setItem("typego_s3_secret_key",secretKey);sessionStorage.setItem("typego_s3_bucket",bucket);
  const objectKey=$("s3ObjectKey").value.trim();
  if(!objectKey){uploadMessage("请填写对象路径",true);return;}
  $("uploadButton").disabled=true;uploadMessage("正在上传，请勿关闭页面...");
  try{
    const signed=await api("/admin/api/v1/uploads/presign",{method:"POST",body:JSON.stringify({access_key:accessKey,secret_key:secretKey,bucket,object_key:objectKey})});
    const response=await fetch(signed.upload_url,{method:"PUT",body:file});
    if(!response.ok)throw new Error(`RainS3 上传失败 (${response.status})`);
    $("downloadUrl").value=signed.download_url;
    uploadMessage("上传成功，下载链接已回填");
  }catch(error){
    const detail=error instanceof TypeError?"无法连接上传服务，请确认已部署最新版 Go 服务并检查反向代理或 RainS3 CORS 配置":error.message;
    uploadMessage(detail,true);
  }finally{$("uploadButton").disabled=false;}
}

$("releaseForm").addEventListener("submit",async event=>{
  event.preventDefault();
  const payload={id:$("id").value,platform:$("platform").value,arch:$("arch").value,version:$("version").value,download_url:$("downloadUrl").value,changelog:$("changelog").value,mandatory:$("mandatory").checked,published:$("publishedCheck").checked,rollout_percent:Number($("rolloutPercent").value)};
  try{await api("/admin/api/v1/releases",{method:"PUT",body:JSON.stringify(payload)});message("版本已保存");reset();await load();}catch(error){message(error.message,true);}
});
$("releaseList").addEventListener("click",async event=>{
  const editID=event.target.dataset.edit,deleteID=event.target.dataset.delete;
  if(editID)edit(editID);
  if(deleteID&&confirm("确定删除该版本吗？")){try{await api(`/admin/api/v1/releases/${encodeURIComponent(deleteID)}`,{method:"DELETE"});await load();}catch(error){message(error.message,true);}}
});
$("resetButton").addEventListener("click",reset);
$("refreshButton").addEventListener("click",load);
$("platformFilter").addEventListener("change",render);
$("rolloutPercent").addEventListener("input",updateRange);
$("uploadButton").addEventListener("click",upload);
$("s3File").addEventListener("change",()=>{const file=$("s3File").files[0];if(file&&!$("s3ObjectKey").value)$("s3ObjectKey").value=`releases/${$("platform").value}/${file.name}`;});
$("s3AccessKey").value=s3Value("access_key");$("s3SecretKey").value=s3Value("secret_key");$("s3Bucket").value=s3Value("bucket");
$("tokenButton").addEventListener("click",()=>{$("tokenInput").value=token();$("tokenDialog").showModal();});
$("saveTokenButton").addEventListener("click",()=>{sessionStorage.setItem("typego_admin_token",$("tokenInput").value);setTimeout(load,0);});
if(!token())$("tokenDialog").showModal();else load();
