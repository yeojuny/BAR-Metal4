// SPDX-License-Identifier: MIT
// AI-assisted focused tests; only an isolated temporary dependency copy is changed.
'use strict';
const fs=require('node:fs'), path=require('node:path'), os=require('node:os');
const assert=require('node:assert/strict'), crypto=require('node:crypto'), zlib=require('node:zlib');
const {createRequire}=require('node:module');
const {apply}=require('./apply-replay-parser-patch.cjs');
const root=path.resolve(__dirname,'..'), req=createRequire(path.join(root,'package.json'));
const spec=JSON.parse(fs.readFileSync(path.join(__dirname,'replay-parser-patch.json')));
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const sandbox=fs.realpathSync(fs.mkdtempSync(path.join(process.env.REPLAY_PATCH_TEST_TMP || os.tmpdir(),'bar-replay-parser-')));
(async()=>{
 fs.mkdirSync(path.join(sandbox,'tools'));fs.mkdirSync(path.join(sandbox,'node_modules'));
 for(const name of ['package.json','package-lock.json'])fs.copyFileSync(path.join(root,name),path.join(sandbox,name));
 fs.copyFileSync(path.join(__dirname,'replay-parser-patch.json'),path.join(sandbox,'tools/replay-parser-patch.json'));
 for(const name of ['sdfz-demo-parser','node-gzip','recoil-tdf']) {
  let packageRoot=path.dirname(req.resolve(name));
  while(!fs.existsSync(path.join(packageRoot,'package.json')) || JSON.parse(fs.readFileSync(path.join(packageRoot,'package.json'))).name!==name) {const parent=path.dirname(packageRoot);assert.notEqual(parent,packageRoot);packageRoot=parent;}
  assert.equal(JSON.parse(fs.readFileSync(path.join(packageRoot,'package.json'))).name,name);
  fs.cpSync(packageRoot,path.join(sandbox,'node_modules',name),{recursive:true,dereference:true});
 }
 const target=path.join(sandbox,'node_modules/sdfz-demo-parser/dist/script-parser.js');
 assert.equal(hash(spec.original),spec.originalSHA256);fs.writeFileSync(target,spec.original);
 const DemoBefore=require(path.join(sandbox,'node_modules/sdfz-demo-parser')).DemoParser;
 const fixtures=path.join(__dirname,'replay-parser-fixtures');
 const old=path.join(fixtures,'old-zero-id.barreplay');
 const before=await new DemoBefore().parseDemo(old);
 assert.equal(before.header.gameId,'0'.repeat(32));
 await assert.rejects(new DemoBefore().parseDemo(path.join(fixtures,'ais-metadata-zero-id.barreplay')),/allyTeamId/);
 assert.throws(()=>apply(sandbox,true),/patch missing/);
 assert.equal(apply(sandbox).status,'patched');assert.equal(apply(sandbox).status,'already-patched');apply(sandbox,true);
 for(const key of Object.keys(require.cache))if(key.startsWith(path.join(sandbox,'node_modules/sdfz-demo-parser')))delete require.cache[key];
 const Demo=require(path.join(sandbox,'node_modules/sdfz-demo-parser')).DemoParser;
 const Script=require(target).ScriptParser;
 assert.deepEqual(await new Demo().parseDemo(old),before);
 const ais=await new Demo().parseDemo(path.join(fixtures,'ais-metadata-zero-id.barreplay'));
 assert.equal(ais.info.ais.length,0);assert.equal(ais.info.players.length,1);assert.equal(ais.header.gameId,'0'.repeat(32));
 await assert.rejects(new Demo().parseDemo(path.join(fixtures,'empty.barreplay')),/unexpected end of file/);
 const raw=zlib.gunzipSync(fs.readFileSync(old));raw.fill(0x12,280,296);
 const synthetic=path.join(sandbox,'synthetic-old-header-nonzero-id.barreplay');fs.writeFileSync(synthetic,zlib.gzipSync(raw));
 assert.equal((await new Demo().parseDemo(synthetic)).header.gameId,'12'.repeat(16));
 const truncated=path.join(sandbox,'truncated.barreplay');fs.writeFileSync(truncated,fs.readFileSync(old).subarray(0,20));
 await assert.rejects(new Demo().parseDemo(truncated),/unexpected end of file/);
 const script=s=>Buffer.from(s.replace(/([{};])/g,'$1\n').replace(/\{/g,'\n{'));
 const valid=new Script({}).parseScript(script('[game]{[allyteam2]{}[team7]{allyteam=2;}[ai9]{team=7;}[player3]{team=7;}[player4]{spectator=1;}[ais]{}[teamoptions]{}[playeroptions]{}[allyoptions]{}}'));
 assert.equal(valid.ais.length,1);assert.equal(valid.players.length,1);assert.equal(valid.spectators.length,1);assert.equal(valid.ais[0].allyTeamId,2);
 for(const section of ['ai1','player1'])assert.throws(()=>new Script({}).parseScript(script('[game]{['+section+']{team=8;}}')),/references missing team 8/);
 fs.appendFileSync(target,'\n// unexpected change');assert.throws(()=>apply(sandbox),/Unexpected parser bytes/);
 fs.writeFileSync(target,spec.replacement);const lockPath=path.join(sandbox,'package-lock.json');const lock=JSON.parse(fs.readFileSync(lockPath));lock.packages['node_modules/sdfz-demo-parser'].integrity='wrong';fs.writeFileSync(lockPath,JSON.stringify(lock));assert.throws(()=>apply(sandbox),/lock pin differs/);
 console.log(JSON.stringify({status:'PASS',checks:['fresh install patch','idempotence','check-only missing patch rejection','unknown source refusal','wrong lock integrity refusal','old replay unchanged full parse','empty ais metadata corrected without invented bots','zero game IDs retained','synthetic nonzero-ID old header accepted','empty and truncated gzip rejected','exact section names','spectator preserved','missing actual teams explicitly rejected'],compilerRuns:0,appRuns:0}));
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(()=>fs.rmSync(sandbox,{recursive:true,force:true}));
